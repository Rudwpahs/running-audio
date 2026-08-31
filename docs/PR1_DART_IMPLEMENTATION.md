# PR1-DART host reference implementation

This document records the host-testable implementation for issues #24–#32. It is deliberately split from the hardware integration path so adaptive features can remain **default-off** until the fixed-channel SX1280 baseline is measured.

## Hard protocol gate

`firmware/common/pr1_packet.hpp` now enforces the **SX1280 FLRC 127-byte payload ceiling**. The previous pre-study 176-byte PCM packet is retained only as a legacy geometry constant and is explicitly too large. The PR1-DART baseline is:

- 48 kHz media clock
- 10 ms frame
- target Opus payload: 100 B
- existing PR1 header: 16 B
- total application packet: **116 B**

The 16-byte header is intentionally retained in this host reference to avoid a simultaneous wire-format redesign. It can be compacted later if measured airtime requires it.

## Implemented modules

### #24 Instrumentation
`pr1_instrumentation.hpp`
- fixed-size trace ring, no allocation in hot path
- named audio/radio/FEC/ARQ/PLC events
- fixed duration windows with percentile queries
- CRC, missing, scheduler, recovery and queue-depth counters

Hardware-only remaining gate: wire these timestamps to the actual ESP32-S3 ISR/SPI/I2S/Opus path and measure IRQ→SPI and E2E latency.

### #25 AFH core
`pr1_afh.hpp`
- 40 channels, 2404–2482 MHz, 2 MHz spacing
- deterministic session-seeded permutation
- no identical consecutive channel, including epoch boundaries
- active bitmap + map version + future-sequence activation
- wrap-safe sequence/version comparison
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
- rejects ambiguous groups

### #28 Deadline ARQ
`pr1_arq.hpp`
- compact 10-byte feedback record
- highest sequence, 32-bit loss bitmap, RSSI, map version, buffer level
- one-shot repair decision based on remaining playout slack and per-frame airtime budget

### #29 Jitter/PLC path
`pr1_jitter.hpp`
- fixed allocation reorder buffer
- 40 ms default target
- sequence-derived deadlines at 10 ms/frame
- stale packet rejection
- recovery ordering: original → XOR → ARQ → optional Opus FEC → PLC

Hardware-only remaining gate: actual ESP32-S3 Opus complexity 0/3/5/7/10 benchmark and decoder/PLC wiring.

### #30 PHY ladder
`pr1_phy.hpp`
- 1.3M/3/4 → 650k/3/4 → 520k/3/4
- 325k emergency profile represented but not auto-selected
- local interference vs global weak-link vs burst classifier
- cooldown-controlled ladder transitions
- host airtime estimator marked planning-only

### #31 Cross-layer controller
`pr1_link_controller.hpp`
- GOOD / INTERFERENCE / WEAK_LINK / BURST / RECOVERY
- minimum dwell and recovery hold
- feature flags
- airtime guard prevents stacked repair overload
- fixed transition history

### #32 Validation
- `tests/run_host_tests.sh`: warning-clean build plus ASan/UBSan
- `tools/pr1_dart_sim.py`: deterministic A–H ablation matrix
- GitHub Actions workflow runs both

## Activation order on hardware

1. Keep AFH/FEC/ARQ/controller OFF.
2. Integrate instrumentation only and reproduce fixed-channel baseline.
3. Enable deterministic hopping with a static all-channel map.
4. Add channel-quality map adaptation.
5. Add XOR FEC.
6. Add deadline ARQ.
7. Add jitter/PLC path and Opus benchmark.
8. Add PHY ladder.
9. Enable cross-layer controller last.
10. Run the issue #32 field matrix before calling the integrated stack validated.

## What this implementation does not claim

Host tests prove deterministic logic, bounds and memory safety of the reference modules. They **do not** prove RF range, legal RF settings, ESP32-S3 real-time deadlines, audio quality, power draw or field reliability. Those require the two physical SX1280 boards and the issue #32 measurement matrix.
