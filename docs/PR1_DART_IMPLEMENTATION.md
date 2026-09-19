# PR1-DART host reference implementation

This document records the host-testable implementation for issues #24–#32 plus the #40/#41 pre-activation safety hardening and the first gated T3-S3/SX1280 fixed-channel runtime integration. Adaptive features remain **default-off** until the fixed-channel SX1280 baseline and receiver timing behavior are physically measured.

## Hard protocol gate

`firmware/common/pr1_packet.hpp` enforces the **SX1280 FLRC 127-byte payload ceiling**. The previous pre-study 176-byte PCM packet is retained only as a legacy geometry constant and is explicitly too large. The PR1-DART baseline remains:

- 48 kHz media clock
- 10 ms frame
- target Opus payload: 100 B
- existing PR1 header: 16 B
- total application packet: **116 B**

The safety-hardening work does **not** expand the wire sequence or packet header. `sequence` remains `uint16_t` on air; long-lived identity is extended only inside the endpoint.

## Implemented modules

### Shared logical frame identity
`pr1_sequence.hpp`
- maps compact wire `uint16_t` sequences onto a monotonic `uint64_t` logical frame timeline
- explicitly rejects exact half-range ambiguity (`0x8000`) rather than guessing order
- supports normal reorder plus repeated `65535 → 0` wraps without moving old frames into the future
- keeps session generation separate from the wire field so local stale state can be flushed on explicit session reset
- adds no RF bytes and no dynamic allocation

### #24 Instrumentation / telemetry
`pr1_instrumentation.hpp`, `pr1_telemetry.hpp`
- fixed-size trace ring, no allocation in hot path
- named audio/radio/FEC/ARQ/PLC events
- fixed duration windows with percentile queries
- CRC, missing, scheduler, recovery and queue-depth bookkeeping counters
- host-visible measurement fields are observation-aware: **unobserved is not zero**
- safe RF-disabled runtime therefore does not report fake zero RF loss/CRC/scheduler measurements
- additive live-runtime timing field `spi_duration_us` uses field ID `0x15`; existing field IDs remain unchanged

The fixed-channel runtime now wires DIO receive timestamps, SPI read timing, RX-processing timing, RX re-arm timing, RSSI, packet outcome, missing-sequence accounting, queue depth and scheduler-miss bookkeeping into the live telemetry path. Continuous event streaming is intentionally disabled during this gate; events remain in the fixed-size in-memory trace ring and `PR1T` snapshots are pulled on demand so serial I/O does not become the receiver bottleneck.

Hardware-only remaining gate: capture those measurements on the physical T3-S3/SX1280 pair and determine the actual receiver-processing and RF behavior. I2S/Opus/E2E audio timing remains a later gate.

### #25 AFH core
`pr1_afh.hpp`
- 40 channels, 2404–2482 MHz, 2 MHz spacing
- deterministic session-seeded permutation
- no identical consecutive channel, including epoch boundaries
- active bitmap + map version + future logical-frame activation
- channel scheduling and pending activation use the shared `uint64_t` logical frame timeline, including beyond a 32-bit frame counter
- three-band rendezvous channels for resync
- session beacon matching
- `PR1_ENABLE_AFH=0` by default

### #26 Channel quality
`pr1_channel_quality.hpp`
- fast/slow PDR EWMA (default 1/4 and 1/32)
- ACTIVE → SUSPECT → EXCLUDED → PROBE
- minimum 12 active channels
- 200 ms probe start, exponential backoff capped at 3.2 s
- reinclude after at least 2 successes in last 3 probes

### #27 XOR FEC
`pr1_fec.hpp`
- fixed 100-byte codec payload XOR
- 4+1 baseline and template support for 3+1 experiment
- exactly one missing source can be reconstructed
- 16-bit parity `group_id` remains on wire for compatibility but is only a consistency check
- long-lived recovery uses session-aware logical group identity, so stale parity cannot become current after group-ID wrap
- parity payload remains 104 B; no RF expansion

### #28 Deadline ARQ
`pr1_arq.hpp`
- compact 10-byte feedback record remains unchanged
- highest sequence, 32-bit loss bitmap, RSSI, map version, buffer level
- repair eligibility remains based on NACK, feedback freshness, map version, playout slack, ETA/guard, airtime budget and an ACTIVE repair channel
- one-shot tracker stores full logical frame identity instead of raw `uint16_t`
- scheduling uses a `Reserved` state; queue/TX failure releases it
- permanent one-shot consumption and `sent` accounting occur only after explicit successful TX commit
- no blind retransmission is introduced

### #29 Jitter/PLC path
`pr1_jitter.hpp`
- fixed-allocation reorder buffer
- 40 ms default target
- logical-frame-derived 64-bit deadlines at 10 ms/frame
- checked/saturating deadline arithmetic removes the former +32768-frame (~5m27.68s) failure horizon
- first/multiple wire-sequence wraps and >1 h logical sessions are host-tested
- stale prior-session frames and delayed old frames remain stale
- recovery ordering: original → XOR → ARQ → optional Opus FEC → PLC

Hardware-only remaining gate: actual ESP32-S3 Opus complexity benchmark and decoder/PLC wiring.

### #30 PHY ladder
`pr1_phy.hpp`
- 1.3M/3/4 → 650k/3/4 → 520k/3/4
- 325k emergency profile represented but not auto-selected
- host airtime estimator remains planning-only

### #31 Cross-layer controller
`pr1_link_controller.hpp`
- GOOD / INTERFERENCE / WEAK_LINK / BURST / **PROCESSING_LIMITED** / RECOVERY
- every adaptive feature flag defaults OFF
- processing-limited classification is valid only when processing measurements are explicitly marked observed and calibrated thresholds are configured
- queue depth, IRQ→SPI, RX processing, RX re-arm and scheduler misses can identify receiver saturation before RF-loss reactions
- PROCESSING_LIMITED keeps the fast baseline PHY and sheds probe/FEC/ARQ work instead of creating the positive-feedback path `RX overload → loss → slower PHY/FEC → more occupancy/work`
- processing thresholds default to zero/unconfigured; hardware Round 4 data must calibrate them
- configured GOOD PER threshold participates in recovery hysteresis
- adaptive jitter changes only when explicitly enabled
- airtime guard still prevents stacked repair overload
- fixed transition history records the classification evidence

### #32 Validation
- `tests/run_host_tests.sh`: warning-clean build plus ASan/UBSan
- sequence/jitter/ARQ/FEC/AFH/controller/telemetry long-session safety regressions run through the same host suite
- `tools/pr1_dart_sim.py`: deterministic A–H **structural regression** matrix only
- GitHub Actions run the host suite, parser tests and packet simulator
- runtime CI builds the RF-disabled `safe` image and compile-gates explicit fixed-FLRC TX and RX profiles against pinned RadioLib

The synthetic A–H matrix and RF compile gates are not RF propagation or real-time load models and are not evidence of physical performance.

## Phase-1 fixed-channel runtime status

The hardware-facing software integration now contains:

- compile-time roles `Safe`, `Tx`, `Rx`
- `safe` as the only default PlatformIO environment
- explicit RF enable gate; RF-enabled builds must select TX or RX
- fixed FLRC 1.3 Mbps / CR 3/4 baseline at one channel
- 116-byte canonical PR1-DART packet
- historical post-transmit `TX_GAP_US` semantics: idle time starts after the blocking TX attempt finishes; `0 us` is valid for an explicit stress point
- TX sequence commit only after successful physical transmit
- host-tested duplicate, forward-gap, malformed-packet, CRC-error and RX re-arm handling
- official LILYGO T3-S3/SX1280 pin mapping through the board config
- RadioLib adapter behind the RF gate
- pull-based live telemetry so serial logging does not contaminate the timing path

This status is **software/compile integration only**. `hardware_verified` remains false until physical evidence is captured.

## Activation order on hardware

1. Keep AFH/FEC/ARQ/controller OFF.
2. Enable only a gated fixed-channel SX1280 runtime after the board/revision gate. **Software integration complete; physical board verification pending.**
3. Integrate live instrumentation and reproduce the existing TX-gap boundary. **Software instrumentation complete; physical sweep pending.**
4. Determine whether losses come from receiver processing, RF weakness, or both using queue/timing/RSSI/PER evidence. **Pending physical measurements.**
5. Enable deterministic hopping with a static all-channel map.
6. Add channel-quality map adaptation.
7. Add XOR FEC.
8. Add deadline ARQ.
9. Add jitter/PLC path and Opus benchmark.
10. Add PHY ladder.
11. Enable the cross-layer controller last, with processing thresholds calibrated from hardware.
12. Run the issue #32 field matrix before calling the integrated stack validated.

The fixed-link physical sequence is: safe boot → RX live-ready → TX live-ready → 100-packet sanity → at least 1,000 fixed-link packets → post-TX gap sweep `500 / 300 / 250 / 225 / 200 / 175 / 150 / 125 us` (with optional explicit `0 us` stress) → loss-source classification from RSSI/PER plus IRQ→SPI/SPI/RX/re-arm/queue/scheduler evidence.

## What this implementation does not claim

Host tests prove deterministic common-code logic, bounds and sanitizer-clean behavior for the exercised cases. Compile gates prove that the safe and fixed-FLRC TX/RX firmware profiles build against the pinned toolchain. They **do not** prove RF range, legal RF settings, ESP32-S3 real-time deadlines, audio quality, power draw, controller threshold quality, or field reliability. Those require the physical SX1280 boards and measured #24/#32/#36 gates.

Local `session_generation` is not authentication. Before a public/study-cafe multi-user deployment, a separate protocol-security design must cover session binding, replay protection, authenticated reverse-link feedback and confidentiality if required by the threat model.
