# PR1 Control / Telemetry + Runtime Instrumentation Design

## Status
Approved architecture record for the post-#38 instrumentation round.

## Context
PR #38 established a minimal ESP32-S3/T3-S3/SX1280 hardware-facing runtime with RF disabled by default. The repository already contains a host-testable `firmware/common/pr1_instrumentation.hpp` with a fixed-size trace ring, duration windows, and baseline counters, but that instrumentation is not yet wired into the runtime or exported in a stable host-readable schema.

A separately collected SHOKZ open-source reference bundle contains two useful design patterns:

1. compact command/device-state handling from an MIT-licensed RFCOMM control project;
2. model/state/telemetry parsing from an MIT-licensed TLV-oriented SHOKZ diagnostic project.

The SHOKZ Bluetooth transport and reverse-engineered SHOKZ command bytes are **not** PR1 dependencies and must not enter the PR1 RF/audio transport path. Only the architectural ideas are reused.

## Goals

1. Define a PR1-owned, transport-neutral control/telemetry vocabulary.
2. Extend the existing fixed-size instrumentation model for the measurements needed to classify the known RX timing collapse.
3. Export deterministic line-oriented telemetry from the safe runtime without initializing or transmitting RF.
4. Add a host parser that can turn runtime telemetry lines into structured records suitable for CSV/JSON analysis.
5. Preserve a future path to low-rate over-the-air control/telemetry without consuming the 11-byte margin inside every 116-byte PR1-DART audio packet.

## Non-goals

- Do not copy SHOKZ RFCOMM code into PR1 firmware.
- Do not implement Bluetooth in PR1.
- Do not enable SX1280 RF in this round.
- Do not add AFH, FEC, live ARQ, Opus, jitter playback, or audio capture.
- Do not claim physical board timing measurements from CI.
- Do not append diagnostics to each PR1-DART audio packet.

## Architectural decision

PR1 owns its own diagnostic schema. SHOKZ material is a reference only.

```text
firmware/common/
  pr1_instrumentation.hpp      existing fixed-size measurement primitives
  pr1_telemetry.hpp            new PR1-owned state/capability/field vocabulary

firmware/t3s3_sx1280_runtime/
  src/main.cpp                 safe boot + deterministic telemetry snapshot

tools/
  pr1_telemetry_parse.py       line parser -> JSON/CSV-friendly records

tests/
  test_telemetry.cpp           C++ schema/snapshot contract
  test_telemetry_parser.py     host parser contract
```

The same semantic fields can later be emitted over Serial, stored in a trace artifact, or carried in a separate low-rate control/telemetry RF packet. Transport is deliberately separated from meaning.

## Control / telemetry vocabulary

### Device state

`DeviceState` is a compact PR1-owned state machine with explicit numeric values:

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

Round 3 safe runtime reports `SafeIdle`. RF-enabled states are defined now only so later runtime code does not need a breaking schema change.

### Capability bits

Capabilities describe features exposed by the current runtime profile. They do not imply that similarly named common host algorithms are active on hardware.

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

The Round 3 safe runtime advertises `TimingTrace` only.

### Snapshot fields

The host-visible snapshot uses stable PR1 field IDs/names:

| ID | Name | Meaning |
|---:|---|---|
| `0x01` | `device_state` | numeric `DeviceState` |
| `0x02` | `rssi_dbm` | last valid packet RSSI; unavailable in safe mode |
| `0x03` | `crc_good` | good packets |
| `0x04` | `crc_bad` | CRC failures |
| `0x05` | `missing` | inferred missing sequence count |
| `0x06` | `queue_depth` | current queue depth |
| `0x07` | `max_queue_depth` | maximum observed queue depth |
| `0x08` | `scheduler_misses` | missed scheduler deadlines |
| `0x09` | `irq_to_spi_us` | latest IRQ-to-SPI latency |
| `0x0A` | `rx_processing_us` | latest RX processing duration |
| `0x0B` | `rx_rearm_us` | latest RX re-arm duration |
| `0x0C` | `trace_overwrites` | trace ring overwrite count |
| `0x0D` | `jitter_depth` | future audio jitter depth |
| `0x0E` | `underruns` | future audio underruns |
| `0x0F` | `arq_retransmit_sent` | repair packets sent |
| `0x10` | `arq_repair_useful` | useful repairs |
| `0x11` | `arq_repair_late` | late repairs |
| `0x12` | `afh_map_version` | future active map version |
| `0x13` | `phy_mode` | future active PHY mode |
| `0x14` | `capability_mask` | active `Capability` bit mask |

Unavailable measurements are omitted instead of encoded with fabricated zero values when zero could mean a real measurement.

### Exact common interfaces

`firmware/common/pr1_telemetry.hpp` defines the following transport-neutral types:

```cpp
inline constexpr std::uint8_t kTelemetrySchemaVersion = 1;

enum class FieldId : std::uint8_t {
  DeviceState = 0x01,
  RssiDbm = 0x02,
  CrcGood = 0x03,
  CrcBad = 0x04,
  Missing = 0x05,
  QueueDepth = 0x06,
  MaxQueueDepth = 0x07,
  SchedulerMisses = 0x08,
  IrqToSpiUs = 0x09,
  RxProcessingUs = 0x0A,
  RxRearmUs = 0x0B,
  TraceOverwrites = 0x0C,
  JitterDepth = 0x0D,
  Underruns = 0x0E,
  ArqRetransmitSent = 0x0F,
  ArqRepairUseful = 0x10,
  ArqRepairLate = 0x11,
  AfhMapVersion = 0x12,
  PhyMode = 0x13,
  CapabilityMask = 0x14,
};

struct OptionalMetric {
  bool available = false;
  std::int64_t value = 0;
};

struct Snapshot {
  DeviceState state = DeviceState::Booting;
  std::uint32_t capability_mask = 0;
  instrumentation::Counters counters{};
  std::uint32_t trace_overwrites = 0;
  OptionalMetric rssi_dbm{};
  OptionalMetric queue_depth{};
  OptionalMetric irq_to_spi_us{};
  OptionalMetric rx_processing_us{};
  OptionalMetric rx_rearm_us{};
  OptionalMetric jitter_depth{};
  OptionalMetric underruns{};
  OptionalMetric arq_repair_useful{};
  OptionalMetric arq_repair_late{};
  OptionalMetric afh_map_version{};
  OptionalMetric phy_mode{};
};

struct FieldValue {
  FieldId field;
  std::int64_t value;
};

const char* deviceStateName(DeviceState state);
const char* fieldName(FieldId field);
const char* eventName(instrumentation::Event event);

template <typename Emit>
void forEachSnapshotField(const Snapshot& snapshot, Emit&& emit);
```

`forEachSnapshotField()` emits `FieldValue` entries in ascending `FieldId` order. Always-meaningful fields are emitted even when zero: device state, CRC good/bad, missing, max queue depth, scheduler misses, trace overwrites, ARQ retransmit count, and capability mask. Optional fields are emitted only when `available=true`.

The header uses only fixed-size/POD state and callbacks. It must not introduce `std::vector`, `std::string`, `std::map`, heap allocation, exceptions, or RTTI requirements into the firmware common path.

## Event trace model

The existing `Event` enum already contains most high-level events. It is extended only for timing isolation needed by the known receiver bottleneck:

- `RxPacketOk`
- `RxCrcFail`
- `RxRearmStart`
- `RxRearmDone`
- `QueueDepth`

Existing events such as `DioRxDone`, `IsrEnter`, `SpiReadStart`, `SpiReadEnd`, and `SchedulerMiss` remain the source of truth for trace timing.

No dynamic allocation is allowed in `TraceRing`, event recording, snapshot production, or the eventual radio hot path.

## Host wire format

For MVP bring-up, telemetry is **Serial text**, not RF.

Each record is one ASCII line:

```text
PR1T v=1 t_us=123456 field=crc_bad value=4
```

Trace event example:

```text
PR1E v=1 t_us=123456 seq=841 event=spi_read_start value=0
```

Rules:

- Prefix `PR1T` = snapshot telemetry field.
- Prefix `PR1E` = event trace.
- `v=1` = telemetry schema version, independent of PR1-DART packet version.
- Keys are ASCII and stable.
- Unknown extra keys are ignored by the host parser for forward compatibility.
- Unknown `field` or `event` names are preserved as strings by the host parser so newer firmware logs can still be ingested by an older parser.
- Malformed records are rejected, not partially accepted.
- Output ordering is deterministic for snapshot emission.

This text format is intentionally simple for serial debugging. A future binary TLV transport may map the same `FieldId` values into a separate control packet; that future binary encoding is not implemented in this round.

## Runtime behavior in this round

The `safe` profile remains RF-disabled and still contains no radio initialization.

At boot it prints the existing metadata, then one deterministic baseline telemetry snapshot including:

- `device_state=1` (`SafeIdle`)
- `crc_good=0`
- `crc_bad=0`
- `missing=0`
- `max_queue_depth=0`
- `scheduler_misses=0`
- `trace_overwrites=0`
- `arq_retransmit_sent=0`
- `capability_mask=8` (`TimingTrace`)

It does **not** print RSSI, current queue depth, RX timing, jitter, underrun, ARQ usefulness/late, AFH, or PHY values when those measurements have never been observed.

The runtime owns only line formatting. Semantic field selection remains in `pr1_telemetry.hpp`.

## Host parser

`tools/pr1_telemetry_parse.py` accepts stdin or a file and produces newline-delimited JSON by default.

Public parser function used by tests:

```python
def parse_line(line: str) -> dict | None:
    ...
```

Behavior:

- unrelated serial lines return `None`;
- valid telemetry returns `{"kind":"telemetry","version":1,"t_us":...,"field":"...","value":...}`;
- valid events return `{"kind":"event","version":1,"t_us":...,"seq":...,"event":"...","value":...}`;
- unknown extra key/value tokens are ignored;
- unknown field/event names are preserved;
- malformed PR1-prefixed lines raise `ValueError`;
- integer parsing uses base 10 only;
- timestamps and sequence numbers are preserved exactly;
- CLI `--format jsonl` is default;
- CLI `--format csv` emits telemetry and event rows with a stable superset of columns: `kind,version,t_us,seq,field,event,value`.

The parser is a host tool, not firmware code.

## RX timing classification path

Once RF is enabled in a later gated round, the instrumentation must support a fixed-PHY/fixed-payload sweep at TX gaps:

`500, 300, 250, 225, 200, 175, 150, 125 us`.

For every gap, analysis must correlate:

- packet loss / CRC failures;
- IRQ-to-SPI latency;
- SPI read duration;
- total RX processing duration;
- RX re-arm duration;
- queue depth / max queue depth;
- scheduler misses.

Classification rules:

- If RSSI remains healthy while queue/timing/scheduler metrics degrade with shorter gaps, classify the failure as receiver processing saturation.
- If timing stays healthy but RSSI/CRC behavior degrades with path loss or obstruction, classify as RF/link weakness.
- If both degrade, classify as mixed and do not hide the processing component with FEC/ARQ/jitter buffering.

The current known observation (225-500 us stable, 175-200 us boundary, <=150 us collapse) remains a hypothesis to reproduce, not a product guarantee.

## SHOKZ reference boundary

The reusable SHOKZ material influences only these design choices:

- separate device state from transport;
- compact stable field identifiers;
- model/capability-driven feature exposure;
- parser tolerant of unknown future fields;
- explicit distinction between a command being sent and a state being verified.

The following are explicitly excluded from PR1 implementation:

- SHOKZ RFCOMM channel 14 behavior;
- SHOKZ 9-byte EQ command bytes;
- BlueZ/D-Bus discovery code;
- SHOKZ model codes and EQ mappings;
- SHOKZ proprietary/dongle TLV tag numbers.

Those may be used later only in a separate PR2 experimental backend with MIT attribution preserved.

## Testing strategy

### Host C++ tests

- field IDs and schema version are stable;
- state/field/event names are deterministic;
- event additions preserve fixed-size trace behavior;
- snapshot field order is stable;
- optional unavailable metrics are omitted;
- always-meaningful zero counters are emitted;
- no heap-owning containers are introduced in common telemetry/instrumentation;
- trace overwrite accounting remains correct.

### Python parser tests

- valid telemetry record parses;
- valid event record parses;
- unknown extra keys are tolerated;
- unknown field/event names are preserved;
- missing required keys fail;
- invalid integer fields fail;
- mixed input ignores unrelated serial lines and preserves valid PR1 records;
- CSV header/order is stable.

### Firmware CI

- existing `safe` PlatformIO build remains green;
- `PR1_RF_ENABLED=0` remains enforced;
- no RadioLib/SX1280 initialization is introduced;
- parser unit tests run in CI with Python standard library only;
- existing host/sanitizer and packet-simulator jobs remain green.

## Acceptance criteria

1. `firmware/common/pr1_telemetry.hpp` exists with schema version, state, capability, field IDs, names, and zero-allocation snapshot iteration.
2. Existing instrumentation has the RX timing events required for later bottleneck classification.
3. Safe runtime emits deterministic `PR1T` records while remaining RF-disabled.
4. Host parser and parser tests are green.
5. Full C++ host + ASan/UBSan suite is green.
6. ESP32-S3 PlatformIO `safe` build is green.
7. PR1 packet simulator remains green.
8. No SHOKZ transport/proprietary command code is linked into PR1.

## MVP / Scale / Trade-offs / Failure

### MVP
Serial text telemetry is the smallest observable interface that lets the runtime and host tooling evolve before physical RF measurements. It reuses the existing trace ring rather than replacing it.

### Scale
Semantic field IDs and states are transport-neutral. The same schema can later feed serial logs, test artifacts, a binary control packet, or a UI without changing the radio/audio core.

### Trade-offs
ASCII telemetry is larger than binary TLV and unsuitable for a high-rate over-the-air path, but it is transparent and low-risk for bring-up. Binary RF telemetry is deferred until fixed-channel timing is measured.

### Failure
Unknown fields remain ingestible, malformed records are rejected, unavailable measurements are omitted, SHOKZ protocol changes cannot break PR1, and instrumentation cannot mask RF or receiver timing failures because recovery features remain disabled during classification.
