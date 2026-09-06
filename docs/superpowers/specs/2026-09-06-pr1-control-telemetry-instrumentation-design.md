# PR1 Control / Telemetry + Runtime Instrumentation Design

## Status
Approved architecture record for the post-#38 instrumentation round, updated to match the implemented observation semantics.

## Context
PR #38 established a minimal ESP32-S3/T3-S3/SX1280 hardware-facing runtime with RF disabled by default. The repository already contains host-testable fixed-size instrumentation primitives in `firmware/common/pr1_instrumentation.hpp`.

The reusable SHOKZ source bundle contributed design ideas only:

- separate device state/capabilities from transport;
- compact stable field identifiers;
- tolerate unknown future fields;
- distinguish a value being supported from a value actually being observed.

No SHOKZ Bluetooth/RFCOMM transport, command bytes, model codes, BlueZ code, or proprietary TLV IDs are PR1 dependencies.

## Goals
1. Define a PR1-owned transport-neutral control/telemetry vocabulary.
2. Extend the existing fixed-size instrumentation model with RX events needed to classify the known timing collapse.
3. Export deterministic serial telemetry from the RF-disabled safe runtime.
4. Parse `PR1T`/`PR1E` records into JSONL/CSV on the host.
5. Preserve a future separate low-rate control/telemetry transport without consuming the 11-byte margin inside every 116-byte PR1-DART audio packet.

## Non-goals
- No Bluetooth in PR1.
- No SHOKZ protocol implementation in PR1.
- No SX1280 RF enable in this round.
- No AFH, FEC, live ARQ, Opus, jitter playback, or audio capture.
- No physical RF/timing claim from CI.
- No diagnostics appended to every audio packet.

## Architecture

```text
firmware/common/
  pr1_instrumentation.hpp      fixed-size trace/counter primitives
  pr1_telemetry.hpp            PR1-owned semantic telemetry schema

firmware/t3s3_sx1280_runtime/
  include/pr1_safe_telemetry.hpp
  src/main.cpp                 safe boot + one serial snapshot

tools/
  pr1_telemetry_parse.py       PR1T/PR1E -> JSONL/CSV
```

Meaning is independent from transport. The same semantic fields may later be carried over serial, test artifacts, or a separate binary control packet.

## Device state

```cpp
enum class DeviceState : std::uint8_t {
  Booting = 0,
  SafeIdle = 1,
  Ready = 2,
  Streaming = 3,
  Degraded = 4,
  Fault = 5,
};
```

The current safe runtime reports `SafeIdle`.

## Capability bits

```cpp
enum class Capability : std::uint32_t {
  AudioOut = 1u << 0,
  BatteryTelemetry = 1u << 1,
  RfStats = 1u << 2,
  TimingTrace = 1u << 3,
  Arq = 1u << 4,
  Afh = 1u << 5,
  Fec = 1u << 6,
  AdaptivePhy = 1u << 7,
};
```

The safe runtime advertises `TimingTrace` only. A capability says a subsystem/schema is exposed; it does **not** prove any particular measurement has been observed.

## Field IDs

| ID | Name | Meaning |
|---:|---|---|
| `0x01` | `device_state` | numeric `DeviceState` |
| `0x02` | `rssi_dbm` | last observed RSSI |
| `0x03` | `crc_good` | good packet counter |
| `0x04` | `crc_bad` | CRC-failure counter |
| `0x05` | `missing` | inferred missing-sequence counter |
| `0x06` | `queue_depth` | observed current queue depth |
| `0x07` | `max_queue_depth` | observed maximum queue depth |
| `0x08` | `scheduler_misses` | scheduler-deadline misses |
| `0x09` | `irq_to_spi_us` | observed IRQ-to-SPI latency |
| `0x0A` | `rx_processing_us` | observed RX processing duration |
| `0x0B` | `rx_rearm_us` | observed RX re-arm duration |
| `0x0C` | `trace_overwrites` | trace ring overwrite count |
| `0x0D` | `jitter_depth` | observed jitter depth |
| `0x0E` | `underruns` | observed audio underruns |
| `0x0F` | `arq_retransmit_sent` | observed repair-send count |
| `0x10` | `arq_repair_useful` | observed useful repairs |
| `0x11` | `arq_repair_late` | observed late repairs |
| `0x12` | `afh_map_version` | observed/active AFH map version |
| `0x13` | `phy_mode` | observed/active PHY mode |
| `0x14` | `capability_mask` | active capability bits |

## Observation semantics
A central rule is **supported is not the same as observed**.

`OptionalMetric` is used whenever `0` could be mistaken for a real observation:

```cpp
struct OptionalMetric {
  bool available = false;
  std::int64_t value = 0;
};
```

`Snapshot` contains always-defined state/counters plus optional observations:

```cpp
struct Snapshot {
  DeviceState state = DeviceState::Booting;
  std::uint32_t capability_mask = 0;
  instrumentation::Counters counters{};
  std::uint32_t trace_overwrites = 0;

  OptionalMetric rssi_dbm{};
  OptionalMetric queue_depth{};
  OptionalMetric max_queue_depth{};
  OptionalMetric irq_to_spi_us{};
  OptionalMetric rx_processing_us{};
  OptionalMetric rx_rearm_us{};
  OptionalMetric jitter_depth{};
  OptionalMetric underruns{};
  OptionalMetric arq_retransmit_sent{};
  OptionalMetric arq_repair_useful{};
  OptionalMetric arq_repair_late{};
  OptionalMetric afh_map_version{};
  OptionalMetric phy_mode{};
};
```

`forEachSnapshotField()` emits fields in ascending `FieldId` order.

Always emitted for the current baseline contract:
- `device_state`
- `crc_good`
- `crc_bad`
- `missing`
- `scheduler_misses`
- `trace_overwrites`
- `capability_mask`

Observation-dependent fields are emitted **only** when their corresponding `OptionalMetric.available == true`. This includes `max_queue_depth` and `arq_retransmit_sent`; values in legacy/common counters do not by themselves prove that those subsystems were active or observed in the hardware runtime.

The common header remains fixed-size/POD and introduces no `std::vector`, `std::string`, `std::map`, heap allocation, exceptions, or RTTI requirement into the firmware path.

## Event trace model
The existing trace vocabulary is extended only with:
- `RxPacketOk`
- `RxCrcFail`
- `RxRearmStart`
- `RxRearmDone`
- `QueueDepth`

Existing DIO/ISR/SPI events remain the source of truth for timing. Defining an event name does not mean a physical event has already been captured.

## Serial wire format

Telemetry:
```text
PR1T v=1 t_us=123456 field=crc_bad value=4
```

Event:
```text
PR1E v=1 t_us=123456 seq=841 event=spi_read_start value=0
```

Rules:
- `v=1` is telemetry schema version, independent from PR1-DART packet version.
- unknown extra key/value tokens are ignored by the host parser;
- unknown field/event names are preserved;
- malformed `PR1T`/`PR1E` records are rejected;
- unrelated serial lines are ignored;
- snapshot field order is deterministic.

## Safe runtime behavior
The only profile remains `PR1_RF_ENABLED=0`.

The safe runtime emits one boot snapshot with fields meaningful without a live queue/RF/recovery path:

```text
PR1T v=1 t_us=<same timestamp> field=device_state value=1
PR1T v=1 t_us=<same timestamp> field=crc_good value=0
PR1T v=1 t_us=<same timestamp> field=crc_bad value=0
PR1T v=1 t_us=<same timestamp> field=missing value=0
PR1T v=1 t_us=<same timestamp> field=scheduler_misses value=0
PR1T v=1 t_us=<same timestamp> field=trace_overwrites value=0
PR1T v=1 t_us=<same timestamp> field=capability_mask value=8
```

It does not fabricate RSSI, queue, RX timing, jitter, underrun, ARQ, AFH, or PHY observations.

## Host parser
`tools/pr1_telemetry_parse.py` provides:

```python
def parse_line(line: str) -> dict | None:
    ...
```

Behavior:
- unrelated lines return `None`;
- malformed PR1-prefixed records raise `ValueError`;
- `v`, `t_us`, `seq`, and `value` use strict base-10 integer parsing;
- unknown extra keys are ignored;
- unknown field/event names are preserved;
- JSONL is default;
- CSV columns are fixed as `kind,version,t_us,seq,field,event,value`;
- missing input files and malformed records fail cleanly without leaking a Python traceback.

## RX timing classification path
After the separate physical RF gate is satisfied, run fixed-payload/fixed-PHY TX-gap sweeps at:

`500, 300, 250, 225, 200, 175, 150, 125 us`

Correlate:
- packet loss / CRC;
- RSSI;
- IRQ-to-SPI latency;
- SPI duration;
- total RX processing;
- RX re-arm duration;
- queue depth / max queue depth;
- scheduler misses.

Classification:
- healthy/non-worsening RSSI + degrading timing/queue/scheduler metrics => receiver processing saturation;
- healthy timing + degrading RF/RSSI/CRC behavior => RF/link weakness;
- both degrade => mixed;
- required metrics absent => insufficient evidence.

The previously observed gap behavior remains a hypothesis to reproduce, not a product guarantee.

## Testing and acceptance
Required before merge:
- telemetry field/state/event IDs/names stable;
- optional unavailable measurements omitted;
- observed optional values emitted when explicitly marked available;
- trace ring overwrite behavior preserved;
- malformed parser inputs covered;
- full C++ normal + ASan/UBSan suite green;
- Python parser suite green;
- ESP32-S3 PlatformIO `safe` build green;
- packet simulator green;
- no SHOKZ/Bluetooth/RFCOMM/RadioLib/RF-enable code introduced.

## MVP / Scale / Trade-offs / Failure
**MVP:** serial text is the smallest transparent measurement interface and reuses the existing fixed-size instrumentation.

**Scale:** semantic field IDs are transport-neutral and can later feed serial logs, artifacts, binary control packets, or UI tooling.

**Trade-offs:** ASCII is larger than binary TLV and is therefore intentionally kept out of the per-audio-packet RF hot path.

**Failure:** unavailable observations are omitted, malformed records are rejected, unknown future fields stay ingestible, and third-party SHOKZ protocol changes cannot break PR1 core behavior.
