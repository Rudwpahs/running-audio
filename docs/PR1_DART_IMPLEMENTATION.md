# PR1-DART host reference implementation

This document records the host-testable implementation for issues #24–#32 plus the #40/#41 pre-activation safety hardening. It is deliberately split from the hardware integration path so adaptive features can remain **default-off** until the fixed-channel SX1280 baseline and receiver timing behavior are measured.

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

Hardware-only remaining gate: wire timestamps and measurement availability to the actual ESP32-S3 ISR/SPI/I2S/Opus path and measure IRQ→SPI and E2E latency.

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
- GitHub Actions run the host suite, parser tests, packet simulator and RF-disabled PlatformIO build

The synthetic A–H matrix is not an RF propagation or real-time load model and is not evidence of physical performance.

## Activation order on hardware

1. Keep AFH/FEC/ARQ/controller OFF.
2. Enable only a gated fixed-channel SX1280 runtime after the board/revision gate.
3. Integrate live instrumentation and reproduce the existing TX-gap boundary.
4. Determine whether losses come from receiver processing, RF weakness, or both using queue/timing/RSSI/PER evidence.
5. Enable deterministic hopping with a static all-channel map.
6. Add channel-quality map adaptation.
7. Add XOR FEC.
8. Add deadline ARQ.
9. Add jitter/PLC path and Opus benchmark.
10. Add PHY ladder.
11. Enable the cross-layer controller last, with processing thresholds calibrated from hardware.
12. Run the issue #32 field matrix before calling the integrated stack validated.

## What this implementation does not claim

Host tests prove deterministic common-code logic, bounds and sanitizer-clean behavior for the exercised cases. They **do not** prove RF range, legal RF settings, ESP32-S3 real-time deadlines, audio quality, power draw, controller threshold quality, or field reliability. Those require the physical SX1280 boards and measured #24/#32/#36 gates.

Local `session_generation` is not authentication. Before a public/study-cafe multi-user deployment, a separate protocol-security design must cover session binding, replay protection, authenticated reverse-link feedback and confidentiality if required by the threat model.
