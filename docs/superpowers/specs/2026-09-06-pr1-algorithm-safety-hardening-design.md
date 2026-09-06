# PR1 Algorithm Safety Hardening Design

## Status
Approved architecture direction from the 2026-09-06 safety audit. This specification is the implementation contract for the pre-activation hardening work tracked by #40 and #41.

## Context
Current host CI is green, but the safety audit found multiple classes of failures that can pass short component tests and still break long-running or real hardware operation.

The highest-severity confirmed defect is in `pr1_jitter.hpp`: a permanent raw `uint16_t` anchor is converted through signed 16-bit relative arithmetic. At +32768 10 ms frames, the relative delta changes sign, causing deadlines to jump back to the old anchor. That makes valid packets stale after about 327.68 s unless the system re-anchors.

Related risks share the same root pattern:

- ARQ one-shot tracking stores raw 16-bit sequence values, so a legitimate later frame can collide with a stale tracker entry after sequence wrap.
- FEC has a 16-bit group identifier and must not use that value alone as a long-lived freshness identity.
- AFH activation and scheduling must use the same logical frame timeline as audio/recovery logic.
- controller inputs expose RX-processing metrics, but the current classifier can still interpret receiver processing saturation as RF degradation.
- telemetry currently allows some inactive RF counters to appear as observed zeroes.

The common root cause is that short wire identifiers, long-lived logical identity, processing health, and measurement availability are not separated strongly enough.

## Goals
1. Preserve the current compact PR1-DART wire packet format and 16-bit RF sequence.
2. Introduce one long-lived internal logical frame identity that survives 16-bit sequence wrap.
3. Use that identity consistently for jitter, ARQ, AFH activation/scheduling, and FEC freshness checks.
4. Make jitter deadlines safe for multi-hour sessions.
5. Make ARQ one-shot state refer to a logical frame, not a raw 16-bit value.
6. Separate ARQ decision/reservation from successful TX commit.
7. Add explicit receiver-processing-overload classification before enabling robust/slower RF actions.
8. Make adaptive controller features default-off until hardware A/B gates enable them.
9. Enforce `supported != observed` telemetry semantics.
10. Add wrap, soak, restart, failure and overload regression tests before live audio/controller activation.

## Non-goals
- No live SX1280 enablement in this change.
- No Opus/audio capture integration.
- No change to the 16-byte PR1-DART application header.
- No larger RF packet sequence field.
- No final security/session-authentication protocol; that remains a separate blocker before public or multi-user deployment.
- No hard-coded physical RX timing thresholds claimed as validated before hardware measurements.

## Design principles

### 1. Wire identity stays compact; internal identity is extended
The on-air sequence remains `uint16_t` to preserve packet size. Every long-lived internal subsystem uses a logical monotonic frame index.

```text
wire:      stream_id:uint16 + sequence:uint16
                         |
                         v
                SequenceUnwrapper
                         |
                         v
internal:  session_generation + logical_frame:uint64
```

A short wire value must never be used as the sole freshness key for a multi-minute session.

### 2. One moving reference, never a permanent raw-sequence anchor
The unwrapper maps each raw sequence to the nearest logical sequence around a moving logical reference. The permanent-anchor pattern that caused #40 is forbidden.

### 3. Session boundaries are explicit
Starting a new stream/session resets the sequence unwrapper, jitter buffer, ARQ reservations/tracker state, FEC recovery state and pending AFH activation state.

For this hardening round, callers must assign a new `stream_id` at a new PR1 audio session. Reusing an old `stream_id` without an explicit reset is invalid. Cryptographic session binding/replay protection is deferred to the separate security design.

### 4. Failure states must be representable
An ambiguous or implausibly distant sequence is not silently coerced into a valid frame. The caller receives an explicit invalid/ambiguous result and must resync or drop it.

### 5. No new heap allocation in firmware hot paths
All state remains fixed-size/POD-friendly. No `std::vector`, `std::map`, dynamic allocation, exceptions, or RTTI is required by the common firmware path.

## Logical frame identity

### Types
The common layer introduces a small sequence/identity primitive, conceptually:

```cpp
using LogicalFrameIndex = std::uint64_t;

struct LogicalFrameId {
  std::uint32_t session_generation = 0;
  LogicalFrameIndex index = 0;
};
```

`session_generation` is a local runtime generation counter that increments on explicit session reset/start. `stream_id` remains the wire-visible stream discriminator; the generation protects local state from accidental reuse across resets.

The common helper does not add these bytes to the RF packet.

### Sequence unwrapping
The unwrapper keeps the latest accepted logical frame index and its low 16 bits.

For a new raw sequence:

1. Compute the unsigned 16-bit modular delta from the current reference.
2. `0x8000` is exactly half the sequence space and is therefore ambiguous; reject it.
3. Convert the other modular deltas to the nearest signed displacement around the current logical reference.
4. Produce a logical candidate without mutating state.
5. The caller validates the candidate against its reorder/forward window.
6. Only accepted forward progress updates the unwrapper reference.

This permits normal `65535 -> 0` wrap while keeping delayed packets behind the current logical position.

The core unwrapper should not hide policy inside one universal hard-coded reorder threshold. Jitter, feedback and synchronization layers have different valid windows. The helper returns the nearest candidate; each consumer applies its own bounded acceptance rule.

### Half-range ambiguity
A raw difference of exactly 32768 cannot be uniquely ordered in a 16-bit modular space. It must not be accepted as forward progress. This replaces the current accidental `int16_t` interpretation with explicit behavior.

## Monotonic time
Long-session deadline code uses `uint64_t` microseconds in the host/common API.

The hardware runtime must later adapt the platform clock into a monotonic 64-bit value before feeding jitter/ARQ logic. No algorithm may compute multi-hour playout deadlines from an unextended 32-bit absolute microsecond timestamp.

Wrap-safe 32-bit deltas may still be used at a hardware boundary, but absolute common-layer deadlines are 64-bit.

## Jitter buffer redesign

### Anchor
`setAnchor()` becomes logical-frame based:

```text
anchor_logical_frame
anchor_playout_us:uint64
```

Deadline calculation is:

```text
deadline = anchor_playout_us +
           (logical_frame - anchor_logical_frame) * 10,000 us
```

with checked/saturating arithmetic rather than signed-16 sequence math.

### Entries
Buffer slots store logical frame identity. Raw 16-bit sequence may be retained only for diagnostics/wire correlation.

### Stale and duplicate handling
A packet is stale if either:

- its logical frame is already behind the playout cursor/window, or
- its arrival time is at/after the computed logical deadline.

A delayed pre-wrap packet must remain old after a later wrap; it cannot become current merely because its low 16 bits match.

### Session reset
Jitter state is flushed on session change. A packet from a previous generation is rejected.

## ARQ redesign

### Feedback wire compatibility
The current compact feedback packet may keep `rx_highest_seq:uint16` and the recent-loss bitmap. The TX side maps this raw sequence into the active logical session around its current logical transmit reference.

Feedback that cannot be unwrapped consistently into the current active session/window is stale/invalid and cannot trigger repair.

### One-shot tracker
`RetransmissionTracker` stores logical frame identity, not raw sequence. A frame with the same low 16-bit sequence in a later wrap is a different frame.

### Decision, reservation, commit
The current one-shot flow must distinguish these states:

```text
requested -> eligible -> reserved -> queued/TX -> committed_sent
                       \-> enqueue/TX failure -> reservation released
```

Rules:

- eligibility evaluation remains pure;
- reservation prevents duplicate concurrent scheduling;
- `sent` statistics and permanent one-shot consumption occur only after queue/TX success is confirmed;
- enqueue/TX failure releases the reservation so a still-valid repair may be retried if policy and deadline allow;
- a committed frame can never be retransmitted twice.

No blind retransmission is introduced.

## FEC logical grouping
The 16-bit FEC `group_id` remains on-wire for compatibility, but it is not the sole freshness key.

Internally:

```text
logical_fec_group = logical_source_frame / source_count
```

The receiver associates parity with the expected logical source group using the parity packet's outer stream/sequence position plus the low 16-bit group field as a consistency check.

A stale parity frame from a previous 16-bit group wrap must not reconstruct a current group solely because the low group IDs match.

No parity payload expansion is required in this hardening round.

## AFH logical scheduling
AFH scheduling/activation uses the same logical frame timeline.

- channel selection accepts logical frame index;
- pending map activation is keyed to a logical activation frame;
- map activation cannot regress on 16-bit audio sequence wrap;
- the deterministic permutation remains reproducible for TX and RX when given the same session seed/map/logical frame.

AFH stays disabled by default until its hardware gate is passed.

## Controller safety redesign

### Feature defaults
All adaptive controller actions default OFF:

- adaptive map OFF
- XOR FEC OFF
- deadline ARQ action OFF
- adaptive PHY OFF
- adaptive jitter OFF
- probing OFF

A subsystem may exist in common code without being enabled in runtime.

### New processing-limited state
The controller gains an explicit processing-overload classification before RF degradation actions.

Conceptual state set:

```text
GOOD
INTERFERENCE
WEAK_LINK
BURST
PROCESSING_LIMITED
RECOVERY
```

`PROCESSING_LIMITED` is entered only when processing-health inputs are marked valid and configured thresholds are exceeded, for example:

- radio queue depth / max queue depth
- scheduler misses
- IRQ-to-SPI p99/max
- RX processing duration
- RX re-arm duration

Threshold values remain configuration, not physical truth. Hardware Round 4 measurements tune them.

### Processing-limited actions
When processing-limited:

1. do not automatically select a slower PHY merely because PER/burst increased;
2. shed optional load first: probe traffic, repair traffic, FEC work where safe;
3. increase pacing/scheduling margin at the runtime layer rather than masking saturation with more recovery traffic;
4. preserve observability and record the reason for state transition;
5. return toward RF adaptation only after processing headroom recovers.

This prevents the positive-feedback path:

```text
RX saturation -> loss -> slower PHY/FEC -> more occupancy/work -> worse saturation
```

### Declared inputs must have semantics
Controller metrics that remain in the public `Metrics` structure must either affect classification/actions or be removed. The interface must not imply that queue/timing/post-recovery metrics are being considered when they are not.

### Recovery threshold
`good_per_permille` must be used as the configured recovery criterion; hysteresis must be test-covered rather than implied by a field that is never consulted.

### Adaptive jitter flag
Jitter target changes occur only when `adaptive_jitter` is enabled.

## Telemetry truth semantics
`device_state` and `capability_mask` remain always available. Measurement values are observation-aware.

For RF/runtime measurements, `0` and `not observed` are different states.

The snapshot projection uses optional availability for fields including:

- CRC good
- CRC bad
- missing
- scheduler misses when the scheduler is not active
- queue depth / max queue depth
- RX timing
- ARQ counters
- jitter/underrun metrics
- AFH/PHY state

The internal `instrumentation::Counters` may still initialize to zero; emission requires an explicit `observed/available` state from the runtime that owns the counter.

The RF-disabled safe runtime therefore must not claim `crc_good=0`, `crc_bad=0` or `missing=0` as measurements.

## Error and resynchronization behavior

### Ambiguous sequence
If the unwrapper sees exact half-range ambiguity, return an explicit error. Do not move the reference.

### Implausible jump
Consumers reject candidates outside their allowed forward/reorder window. A runtime may enter an explicit resync path rather than silently accepting a huge jump.

### Session restart
A session restart flushes:

- sequence unwrap reference
- jitter entries/playout cursor
- ARQ reservations and committed tracker
- FEC pending groups
- pending AFH activation state

### TX failure
ARQ reservation is released on queue/TX failure; no `sent` increment occurs.

### Timer discontinuity
A backwards/non-monotonic platform timestamp cannot be fed directly into deadline logic. The hardware adapter must restore monotonic 64-bit time or flag/reset the session.

## Testing strategy
All implementation follows RED -> GREEN -> REFACTOR.

### Sequence/logical identity tests
- initial mapping
- normal forward frames
- small reorder
- `65535 -> 0`
- multiple wraps
- exact half-range ambiguity
- delayed old frame after wrap
- explicit session reset

### Jitter regression tests
- current near-anchor cases remain valid
- +32767 frames remains valid
- +32768 frames remains valid in the logical model
- first 16-bit wrap remains valid
- >15 minute logical stream
- >1 hour logical stream
- delayed old packet across wrap is stale
- prior-session frame is rejected

Soak tests advance logical frame/time numerically; they do not wait in real time.

### ARQ tests
- existing deadline/freshness/map/budget gates
- one-shot within one logical frame
- same low 16-bit sequence after wrap is allowed as a new logical frame
- stale feedback from previous logical window rejected
- enqueue/TX failure releases reservation
- `sent` increments only on successful commit
- successful committed repair cannot repeat
- session reset clears old one-shot state

### FEC tests
- current one-erasure recovery
- group low-16 wrap does not make stale parity current
- expected logical group mismatch rejects parity

### AFH tests
- deterministic TX/RX channel match
- no adjacent duplicate behavior remains
- logical frame values beyond 65535 and beyond one hour remain deterministic
- pending map activation works across 16-bit audio wrap

### Controller tests
- all adaptive features default OFF
- RF loss with healthy processing may classify RF states
- healthy RSSI + queue/timing/scheduler saturation classifies PROCESSING_LIMITED
- PROCESSING_LIMITED does not automatically slow PHY/add recovery load
- configured good threshold controls recovery
- adaptive jitter flag gates jitter target changes
- missing processing observations do not falsely classify overload

### Telemetry tests
- safe snapshot emits no unobserved RF counters
- observed zero remains representable and emitted
- observed nonzero emitted
- supported capability alone does not mark a metric observed

### Full verification
- all C++ host tests with `-Wall -Wextra -Werror -pedantic`
- ASan + UBSan
- Python parser tests
- packet simulator regression
- PR1-DART structural simulator regression only; not treated as RF-performance evidence
- PlatformIO RF-disabled safe build

## Performance/scale considerations
At 100 audio frames/s, `uint64_t` logical frame arithmetic is negligible compared with codec/radio work and avoids repeated wrap-specific special cases.

No unbounded history is needed. Jitter and tracker structures remain fixed capacity. Logical IDs make bounded fixed-size storage safe across wraps.

A `uint64_t` logical frame index lasts far beyond product lifetime at 100 frames/s.

## Compatibility
- RF packet header remains 16 bytes.
- audio payload target remains 100 bytes.
- total PR1-DART application packet remains 116 bytes.
- current FLRC 127-byte payload gate remains unchanged.
- no Bluetooth/Wi-Fi dependency is introduced.
- current RF-disabled runtime remains RF-disabled.

## Activation gate
The following are blockers before live audio/controller activation:

1. #40 long-session jitter defect resolved with wrap/soak tests.
2. ARQ logical one-shot + TX-commit semantics green.
3. controller defaults OFF and processing-limited state tests green.
4. safe telemetry no longer emits inactive RF counters as observed values.
5. complete host/sanitizer/PlatformIO safe verification green.

After this software gate, the project returns to the physical sequence already defined in #36:

```text
fixed RF + instrumentation -> reproduce RX timing bottleneck ->
ARQ -> AFH -> FEC -> PHY -> controller -> audio
```

No adaptive layer may be called physically validated until its hardware A/B gate is measured.

## Deferred security blocker
Before public/study-cafe multi-user deployment, a separate protocol design must add session authentication/binding, replay protection, authenticated reverse-link feedback, and confidentiality if the threat model requires it. This hardening design deliberately does not invent a cryptographic protocol while the RF runtime is still disabled.
