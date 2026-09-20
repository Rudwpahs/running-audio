# PR1 pre-board RX timing gate

Date: 2026-09-20
Branch: `codex/pr1-preboard-instrumentation-20260920`

## Purpose

This gate exists to identify the fixed-link receiver bottleneck before any recovery or adaptive layer is enabled. No FreeRTOS task split, SPI-speed tuning, DMA, AFH, FEC, ARQ, adaptive PHY, Opus/jitter/PLC, or re-arm-before-read experiment is allowed until the baseline hardware evidence is captured.

## Frozen baseline

- Board family reference: LILYGO T3-S3-MVSRBoard
- Radio: SX1280
- RadioLib: 7.7.1
- Mode: fixed-channel FLRC
- Frequency: 2404 MHz
- Bitrate: 1300 kbps
- Coding rate: FLRC 3/4
- Output power: 0 dBm
- Packet size: 116 bytes
- SPI clock: 2,000,000 Hz, MSB first, SPI mode 0
- Adaptive/recovery layers: off

`PR1_TX_GAP_US` means idle time after the blocking transmit call completes. It is not a packet start-to-start period.

## Required boot evidence

Both live images must print the expected profile before any sweep is accepted. Record at least:

```text
runtime_role=<tx|rx>
rf_enabled=1
sx1280_spi_hz=2000000
frequency_mhz=2404.000
bitrate_kbps=1300
coding_rate=3
output_dbm=0
packet_bytes=116
adaptive_layers=off
```

Do not accept a run when either board reports a different baseline.

## RX telemetry required for every accepted run

- `rssi_dbm`
- `crc_good`
- `crc_bad`
- `missing`
- `queue_depth`
- `max_queue_depth`
- `scheduler_misses`
- `irq_to_spi_us` p99
- `spi_duration_us` p99
- `rx_processing_us` p99
- `spi_end_to_rearm_start_us` p99
- `rx_rearm_us` p99
- `irq_to_rx_ready_us` p99
- `trace_overwrites`

Timing interpretation:

```text
RxDone IRQ
  |-- irq_to_spi_us --------------------> SPI start
  |                                      |
  |                                      |-- spi_duration_us --> SPI end
  |                                                              |
  |                                                              |-- spi_end_to_rearm_start_us --> re-arm start
  |                                                                                              |
  |                                                                                              |-- rx_rearm_us --> RX ready
  |
  |------------------------------------------- irq_to_rx_ready_us ------------------------------------------->|
```

`rx_processing_us` remains an overlapping diagnostic from SPI start to successful packet completion. Do not add it to the non-overlapping A+B+C+D timing sum.

## Physical experiment sequence

### Gate 0 — build/CI

The branch is not ready for hardware until all existing host/sanitizer/parser tests and the safe/TX/RX PlatformIO builds pass.

### Gate 1 — safe boot

Boot the `safe` image first. Confirm the board metadata and that RF is disabled.

### Gate 2 — live bring-up

Flash one board with `rf_rx_compile` and the other with `rf_tx_compile`. Confirm both live profiles and the frozen SPI baseline.

### Gate 3 — conservative sanity

Use `PR1_TX_GAP_US=5000` and run a short sanity capture. Confirm valid packets arrive and request RX telemetry with `t`/`T`.

### Gate 4 — receiver-boundary sweep

Run at least 1,000 packets at each post-TX gap:

```text
1000
500
300
250
225
200
175
150
125 us
```

Use `0 us` only as an explicit final stress point.

For any transition region where loss changes materially, repeat with at least 10,000 packets. Do not compare a 1,000-packet boundary result against a historical 10,000-packet result as if they were equivalent.

## Classification rules

Do not change firmware until the sweep identifies the dominant contributor.

- Large `irq_to_spi_us`: investigate scheduling/wakeup latency; only then consider a dedicated high-priority RF task.
- Large `spi_duration_us`: investigate SPI transaction cost; only then test one SPI-clock change at a time.
- Large `spi_end_to_rearm_start_us`: investigate decode/RSSI/sequence/instrumentation work in the hot path; only then consider queueing raw packets to a separate processing task.
- Large `rx_rearm_us`: inspect the RadioLib/SX1280 re-arm command path and BUSY behavior.
- Large `irq_to_rx_ready_us` with loss transition at the same gap region: receiver processing saturation becomes the leading hypothesis, but retain CRC/RSSI evidence before classifying RF loss out.

## Explicitly deferred

Do not enable or implement as part of this gate:

- Core 0/Core 1 task restructuring
- `configMAX_PRIORITIES - 1` RF task
- SPI DMA
- higher SPI clock
- `startReceive()` before `readData()`
- AFH
- XOR FEC
- deadline-aware ARQ
- adaptive PHY/controller
- audio capture/playback stack

Each deferred item requires evidence from the fixed-link sweep before it becomes an implementation candidate.
