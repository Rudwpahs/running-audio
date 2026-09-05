# PR1 T3-S3 / SX1280 runtime foundation

Round 2 creates the smallest hardware-facing runtime for the current PR1 architecture.

## Safety contract

The only build profile in this round is `safe`.

- `PR1_RF_ENABLED=0`
- no RadioLib dependency
- no SPI radio initialization
- no SX1280 `begin`, receive, or transmit call
- boot prints deterministic metadata and then idles
- CI does not claim physical board verification

Any build with `PR1_RF_ENABLED=1` intentionally fails at compile time until a later round adds an explicit RF-enabled runtime after physical board/revision confirmation.

## Build

```bash
pio run -e safe
```

or from the repository root:

```bash
pio run --project-dir firmware/t3s3_sx1280_runtime -e safe
```

## Boot metadata

Expected keys include:

```text
PR1_RUNTIME_BOOT
runtime_profile=round2-safe
board_family=LILYGO T3-S3-MVSRBoard
board_reference_revision=V1.1 upstream reference
radio_target=SX1280
hardware_verified=0
protocol_version=1
protocol_header_bytes=16
rf_enabled=0
sx1280_cs=7
sx1280_rst=8
sx1280_sclk=5
sx1280_mosi=6
sx1280_miso=3
sx1280_dio1=9
sx1280_busy=36
sx1280_tx_enable=10
sx1280_rx_enable=21
PR1_RUNTIME_SAFE_IDLE
```

## Hardware reference

The pin values are reference values from the official LILYGO `T3-S3-MVSRBoard` repository, upstream commit `840a2e788b3192c4e9bddf0640c1ecaf703c2598`, specifically:

- `libraries/private_library/pin_config.h`
- `examples/SX128x_PingPong_2/SX128x_PingPong_2.ino`
- upstream PlatformIO configuration (`espressif32 @6.5.0`, Arduino framework)

No vendor radio example source is copied into this runtime. The upstream example is used only as a hardware/API reference.

The upstream pin configuration currently selects MVSRBoard V1.1 as its reference revision. That does **not** prove the user's physical boards are V1.1. Before RF is enabled, the exact physical revision/radio variant must be checked against #12/#13.

## Scope exclusions for Round 2

Not implemented here:

- fixed-channel RF TX/RX
- instrumentation
- AFH
- FEC
- ARQ
- adaptive PHY
- audio

These are intentionally deferred so later failures can be isolated instead of hidden by multiple adaptive layers.
