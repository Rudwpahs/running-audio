# PR1 T3-S3 / SX1280 runtime foundation

PR #38 established the smallest hardware-facing runtime for the current PR1 architecture. The follow-up instrumentation round keeps the same RF-disabled safety boundary and adds deterministic host-readable telemetry.

## Safety contract

The only build profile remains `safe`.

- `PR1_RF_ENABLED=0`
- no RadioLib dependency
- no SPI radio initialization
- no SX1280 `begin`, receive, or transmit call
- boot prints deterministic metadata plus one safe telemetry snapshot, then idles
- CI does not claim physical board verification

Any build with `PR1_RF_ENABLED=1` intentionally fails at compile time until a later gated round adds an explicit RF-enabled runtime after physical board/revision confirmation.

## Build

```bash
pio run -e safe
```

or from the repository root:

```bash
pio run --project-dir firmware/t3s3_sx1280_runtime -e safe
```

## Boot metadata

Expected metadata keys include:

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

## Safe telemetry snapshot

After metadata, the runtime emits one schema-versioned `PR1T` snapshot. All fields in the same snapshot share the same `t_us` value.

Example shape:

```text
PR1T v=1 t_us=<boot_timestamp> field=device_state value=1
PR1T v=1 t_us=<boot_timestamp> field=crc_good value=0
PR1T v=1 t_us=<boot_timestamp> field=crc_bad value=0
PR1T v=1 t_us=<boot_timestamp> field=missing value=0
PR1T v=1 t_us=<boot_timestamp> field=max_queue_depth value=0
PR1T v=1 t_us=<boot_timestamp> field=scheduler_misses value=0
PR1T v=1 t_us=<boot_timestamp> field=trace_overwrites value=0
PR1T v=1 t_us=<boot_timestamp> field=arq_retransmit_sent value=0
PR1T v=1 t_us=<boot_timestamp> field=capability_mask value=8
```

`device_state=1` means `safe_idle`; `capability_mask=8` means the runtime exposes the timing/diagnostic schema. RF-only measurements such as RSSI, IRQ→SPI latency, RX processing time, current queue depth and RX re-arm time are **omitted** until they have actually been observed. They are not fabricated as zero.

The schema is defined in `firmware/common/pr1_telemetry.hpp`. The host parser accepts serial logs and emits JSONL or CSV:

```bash
python tools/pr1_telemetry_parse.py serial.log
python tools/pr1_telemetry_parse.py serial.log --format csv
```

Future `PR1E` event records use the same versioned host-visible convention:

```text
PR1E v=1 t_us=<timestamp> seq=<sequence> event=<event_name> value=<integer>
```

Defining an event name does not mean a physical RF event has already been measured; actual DIO/ISR/SPI/re-arm wiring remains behind the RF/hardware gates.

## Hardware reference

The pin values are reference values from the official LILYGO `T3-S3-MVSRBoard` repository, upstream commit `840a2e788b3192c4e9bddf0640c1ecaf703c2598`, specifically:

- `libraries/private_library/pin_config.h`
- `examples/SX128x_PingPong_2/SX128x_PingPong_2.ino`
- upstream PlatformIO configuration (`espressif32 @6.5.0`, Arduino framework)

No vendor radio example source is copied into this runtime. The upstream example is used only as a hardware/API reference.

The upstream pin configuration currently selects MVSRBoard V1.1 as its reference revision. That does **not** prove the user's physical boards are V1.1. Before RF is enabled, the exact physical revision/radio variant must be checked against #12/#13.

## Current scope exclusions

Not implemented in the hardware runtime yet:

- fixed-channel RF TX/RX
- physical DIO/ISR/SPI/re-arm timing capture
- AFH
- FEC
- live ARQ
- adaptive PHY
- audio

These remain deferred so receiver-processing failures can be isolated instead of hidden by multiple adaptive/recovery layers.
