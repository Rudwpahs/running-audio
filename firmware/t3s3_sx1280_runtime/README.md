# PR1 T3-S3 / SX1280 runtime

This runtime connects the host-tested PR1-DART packet/instrumentation layer to the LILYGO T3-S3/SX1280 hardware boundary.

The default build is still RF-disabled. Two explicit non-default compile profiles exist for the first hardware gate: fixed-channel FLRC TX and fixed-channel FLRC RX. AFH, adaptive channel maps, XOR FEC, deadline ARQ, adaptive PHY, the cross-layer controller, Opus/jitter/PLC and audio I/O are intentionally not activated here.

The Superpowers execution/ruling record for this integration is `docs/superpowers/plans/2026-09-14-pr1-live-flrc-runtime-execution.md`.

## Safety and activation contract

Default profile:

- environment: `safe`
- `PR1_RF_ENABLED=0`
- role: `safe`
- no SX1280 SPI/radio initialization
- no RF transmit/receive call
- deterministic boot metadata and safe telemetry only

Explicit live compile profiles:

- `rf_tx_compile`: `PR1_RF_ENABLED=1`, role `tx`
- `rf_rx_compile`: `PR1_RF_ENABLED=1`, role `rx`
- RadioLib pinned to `7.7.1`
- fixed FLRC profile only
- compile success does **not** mean the physical board/radio path has been validated

The compile-time guards reject RF-enabled builds that do not explicitly choose TX or RX, and reject RF-disabled builds that try to select a live role.

## Fixed FLRC baseline

The first live profile is deliberately non-adaptive:

```text
frequency_mhz=2404.000
bitrate_kbps=1300
coding_rate=3        # FLRC CR 3/4
output_dbm=0
tx_gap_us=5000       # idle time AFTER blocking TX completes
packet_bytes=116
adaptive_layers=off
```

`tx_gap_us` is intentionally **not** a packet start-to-start period. It preserves the semantics of the earlier PR1 V4 experiments: the transmitter completes one blocking radio transmission and then waits `TX_GAP_US` before the next attempt. A `0 us` gap is valid and means the next packet may start as soon as the blocking TX call returns and the runtime loop services again.

The default 5 ms gap is a conservative bring-up value. For the receiver-boundary experiment, change `PR1_TX_GAP_US` under `[env:rf_tx_compile]` in `platformio.ini`, rebuild the TX image, and test the requested sweep values one at a time. This keeps the tested gap compiled into the boot profile instead of changing it silently at runtime.

The 116-byte PR1-DART packet is the existing 16-byte PR1 header plus the 100-byte target codec payload and remains below the SX1280 FLRC 127-byte payload ceiling.

## Build

From the repository root:

```bash
# RF-disabled default safety build
pio run --project-dir firmware/t3s3_sx1280_runtime -e safe

# Compile the explicit live roles
pio run --project-dir firmware/t3s3_sx1280_runtime -e rf_tx_compile
pio run --project-dir firmware/t3s3_sx1280_runtime -e rf_rx_compile
```

Do not treat the two `rf_*_compile` builds as field validation. They prove only that the hardware adapter and runtime compile against the pinned library/toolchain.

## Runtime design

`FixedLinkRuntime` is hardware-independent and talks to a narrow `RadioPort` interface. Host tests use a fake radio to inject TX failures, duplicate packets, sequence gaps, malformed packets, CRC failures and deterministic timing. The board adapter implements the same interface with SX1280 + RadioLib.

The RX interrupt path is intentionally minimal. The ISR-side callback records only the receive-complete timestamp/flag. SPI reads, packet decoding, sequence accounting, telemetry updates and RX re-arm all happen later in `tick()`. This is important because the first physical goal is to distinguish RF loss from receiver-processing saturation rather than hide it with recovery layers.

TX sequence numbers are committed only after `radio.transmit()` succeeds. A failed physical TX therefore retries the same sequence instead of silently manufacturing a source-packet gap. Failed attempts still observe the configured post-TX gap so a radio fault cannot create an uncontrolled hot retry loop.

The FLRC port exposes RSSI only. It deliberately does not fabricate an SNR value for FLRC; an unavailable measurement must remain unavailable rather than appear as `0`.

## Live telemetry

The live accumulator can expose:

- RSSI
- valid PR1 / CRC-good count
- physical CRC-failure count
- missing sequence count
- current / maximum pending-event depth
- scheduler misses
- DIO IRQ -> SPI-start time
- SPI read duration
- RX processing time
- RX re-arm time
- trace-ring overwrites

`queue_depth` currently means the bounded **pending RX event depth** of this single-event runtime, not a hardware FIFO depth or a multi-packet software backlog. Because RX is not re-armed until the current event is serviced, this value is normally `0` or `1` and must not be used alone as evidence that the receiver is or is not saturated. The fixed-link classification must use missing sequences together with IRQ→SPI, SPI duration, RX-processing and RX-rearm timing (plus RSSI/CRC evidence); later queued/audio runtimes can give `queue_depth` a richer workload meaning.

`0` and `unobserved` remain different states. A field is emitted only after the owning measurement has actually been observed.

Live serial telemetry is **pull-based** so logging does not become the receiver bottleneck. After boot, send `t` (or `T`) over Serial to emit the current `PR1T` snapshot:

```text
PR1T v=1 t_us=<timestamp> field=crc_good value=<n>
PR1T v=1 t_us=<timestamp> field=missing value=<n>
PR1T v=1 t_us=<timestamp> field=irq_to_spi_us value=<p99_us>
PR1T v=1 t_us=<timestamp> field=spi_duration_us value=<p99_us>
PR1T v=1 t_us=<timestamp> field=rx_processing_us value=<p99_us>
PR1T v=1 t_us=<timestamp> field=rx_rearm_us value=<p99_us>
```

The timing fields currently report the p99 of fixed-size in-memory windows. The hot path performs no dynamic allocation. Trace events are retained in the in-memory trace ring for this first hardware gate; continuous `PR1E` serial streaming is intentionally not enabled because it could perturb the timing being measured.

The host parser remains:

```bash
python tools/pr1_telemetry_parse.py serial.log
python tools/pr1_telemetry_parse.py serial.log --format csv
```

## Safe boot metadata

The safe environment still prints metadata such as:

```text
PR1_RUNTIME_BOOT
runtime_profile=round2-safe
runtime_role=safe
board_family=LILYGO T3-S3-MVSRBoard
radio_target=SX1280
hardware_verified=0
protocol_version=1
protocol_header_bytes=16
rf_enabled=0
PR1_RUNTIME_SAFE_IDLE
```

A live compile prints its explicit role/profile and still reports `hardware_verified=0` until physical evidence exists.

## Hardware reference

The pin values are reference values from the official LILYGO `T3-S3-MVSRBoard` repository, upstream commit `840a2e788b3192c4e9bddf0640c1ecaf703c2598`:

- CS 7
- RST 8
- SCLK 5
- MOSI 6
- MISO 3
- DIO1 9
- BUSY 36
- TX RF-switch 10
- RX RF-switch 21

No vendor radio example source is copied into PR1. The vendor repository is used only as a hardware/API reference.

The reference configuration identifies MVSRBoard V1.1; that remains a reference claim, not proof of the exact physical board revision in hand.

## First physical gate

Use two boards, one TX image and one RX image. Keep all adaptive/recovery layers off.

1. Boot `safe` first and confirm metadata.
2. Boot the RX fixed-FLRC image and confirm `PR1_RUNTIME_LIVE_READY`.
3. Boot the TX fixed-FLRC image and confirm the same fixed profile on both sides.
4. Run a short 100-packet sanity test and request RX telemetry with `t`.
5. Run at least 1,000 packets and record valid/CRC-good, CRC-bad, missing, RSSI and the four RX timing metrics.
6. Reproduce the receiver-boundary sweep at post-TX gaps `500 / 300 / 250 / 225 / 200 / 175 / 150 / 125 us`; the historical `0 us` point remains supported for an explicit stress run.
7. Correlate PER/CRC and RSSI with IRQ->SPI, SPI duration, RX processing, RX re-arm, pending-event depth and scheduler misses. Treat the current `queue_depth` as a 0/1 event-pending indicator, not a backlog metric.
8. Only after the fixed-link loss source is classified should deterministic static-map AFH be activated.

## Still intentionally disabled in the hardware runtime

- deterministic hopping / AFH
- adaptive channel-quality map
- XOR FEC
- deadline-aware ARQ
- jitter buffer / Opus / PLC
- adaptive PHY ladder
- cross-layer controller
- audio capture / playback

Those algorithms already have host-side primitives/tests in `firmware/common/`; this runtime round is the measurement and hardware-integration gate before they are enabled one layer at a time.
