# PR1 T3-S3 / SX1280 runtime

This runtime connects the host-tested PR1-DART packet/instrumentation layer to the LILYGO T3-S3/SX1280 hardware boundary.

The default build is still RF-disabled. Two explicit non-default compile profiles now exist for the first hardware gate: fixed-channel FLRC TX and fixed-channel FLRC RX. AFH, adaptive channel maps, XOR FEC, deadline ARQ, adaptive PHY, the cross-layer controller, Opus/jitter/PLC and audio I/O are intentionally not activated here.

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
tx_period_us=10000
packet_bytes=116
adaptive_layers=off
```

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

TX sequence numbers are committed only after `radio.transmit()` succeeds. A failed physical TX therefore retries the same sequence instead of silently manufacturing a source-packet gap.

## Live telemetry

The live accumulator can expose:

- RSSI
- CRC good / CRC bad
- missing sequence count
- current / maximum pending queue depth
- scheduler misses
- DIO IRQ -> SPI-start time
- SPI read duration
- RX processing time
- RX re-arm time
- trace-ring overwrites

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

The timing fields currently report the p99 of fixed-size in-memory windows. The hot path performs no dynamic allocation.

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

No vendor radio example source is copied into PR1. The vendor repository is used only as a board/API reference.

The reference configuration identifies MVSRBoard V1.1; that remains a reference claim, not proof of the exact physical board revision in hand.

## First physical gate

Use two boards, one TX image and one RX image. Keep all adaptive/recovery layers off.

1. Boot `safe` first and confirm metadata.
2. Boot the RX fixed-FLRC image and confirm `PR1_RUNTIME_LIVE_READY`.
3. Boot the TX fixed-FLRC image and confirm the same fixed profile on both sides.
4. Run a short 100-packet sanity test and request RX telemetry with `t`.
5. Run at least 1,000 packets and record CRC-good, CRC-bad, missing, RSSI and the four RX timing metrics.
6. Reproduce the receiver-boundary sweep separately at TX gaps 500 / 300 / 250 / 225 / 200 / 175 / 150 / 125 us.
7. Only after the fixed-link loss source is classified should deterministic static-map AFH be activated.

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
