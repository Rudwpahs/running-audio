# PR1 Control Telemetry Instrumentation Implementation Plan

> Implementation plan and execution record for the post-#38 instrumentation round.

**Goal:** Add a PR1-owned transport-neutral telemetry schema, extend RX timing instrumentation, emit deterministic RF-disabled runtime telemetry, and parse it on the host without introducing SHOKZ/Bluetooth dependencies.

**Architecture:** Reuse `pr1_instrumentation.hpp`; add `pr1_telemetry.hpp` as the semantic field/state layer; emit serial text only from the safe runtime; parse on the host with Python standard library only.

**Spec:** `docs/superpowers/specs/2026-09-06-pr1-control-telemetry-instrumentation-design.md`

## Global constraints
- `PR1_RF_ENABLED=0` remains the only runtime profile.
- No SX1280/RadioLib initialization, receive, or transmit call.
- No Bluetooth/RFCOMM/BlueZ/SHOKZ command code in PR1.
- No dynamic allocation or heap-owning container in firmware common telemetry/instrumentation.
- Telemetry schema version is `1`.
- PR1-DART payload ceiling remains 127 B and baseline packet 116 B.
- Telemetry is Serial text only; no per-audio-packet diagnostics.
- No physical timing/RF/flash claim from CI.

## Task 1 — Telemetry schema and snapshot contract ✅

**Files**
- `firmware/common/pr1_telemetry.hpp`
- `tests/test_telemetry.cpp`

**TDD evidence**
- RED: test was committed before the header and failed because `pr1_telemetry.hpp` did not exist.
- GREEN: schema/state/field/name/snapshot implementation added and host normal + ASan/UBSan tests passed.

**Delivered contract**
- `DeviceState` and `Capability` enums.
- Stable `FieldId` values `0x01..0x14`.
- `OptionalMetric { available, value }` for observation-dependent measurements.
- `Snapshot` with fixed-size/POD state.
- `forEachSnapshotField()` deterministic ascending field order.
- unknown enum names map to `"unknown"`.

**Availability rule**
Always emitted in the current baseline snapshot contract:
- device state
- CRC good
- CRC bad
- missing
- scheduler misses
- trace overwrites
- capability mask

Observation-dependent metrics, including `max_queue_depth` and `arq_retransmit_sent`, are emitted only when their `OptionalMetric.available` flag is true. Merely having a capability bit or a legacy/common counter value does not prove the measurement was observed.

## Task 2 — RX bottleneck event vocabulary ✅

**Files**
- `firmware/common/pr1_instrumentation.hpp`
- `tests/test_instrumentation.cpp`
- `tests/test_telemetry.cpp`

**TDD evidence**
- RED: tests referenced new events before enum members existed and failed at compile time.
- GREEN: only the required five events were appended and all host tests passed.

**Events added**
- `RxPacketOk`
- `RxCrcFail`
- `RxRearmStart`
- `RxRearmDone`
- `QueueDepth`

Existing enum values were not reordered.

## Task 3 — Safe runtime telemetry ✅

**Files**
- `firmware/t3s3_sx1280_runtime/include/pr1_safe_telemetry.hpp`
- `firmware/t3s3_sx1280_runtime/src/main.cpp`
- `tests/test_runtime_config.cpp`
- `firmware/t3s3_sx1280_runtime/README.md`

**TDD evidence**
- RED: runtime contract test referenced `pr1_safe_telemetry.hpp` before it existed.
- GREEN: `makeSafeTelemetrySnapshot()` was added and verified under normal + sanitizer host builds, then wired into the Arduino runtime.

**Safe snapshot**
`makeSafeTelemetrySnapshot()` returns:
- `DeviceState::SafeIdle`
- capability mask `TimingTrace` (`8`)
- all RF/queue/recovery observations unavailable until actually measured.

The runtime emits one boot snapshot:

```text
PR1T v=1 t_us=<same timestamp> field=device_state value=1
PR1T v=1 t_us=<same timestamp> field=crc_good value=0
PR1T v=1 t_us=<same timestamp> field=crc_bad value=0
PR1T v=1 t_us=<same timestamp> field=missing value=0
PR1T v=1 t_us=<same timestamp> field=scheduler_misses value=0
PR1T v=1 t_us=<same timestamp> field=trace_overwrites value=0
PR1T v=1 t_us=<same timestamp> field=capability_mask value=8
```

No RSSI, queue, IRQ→SPI, RX processing, RX re-arm, ARQ, AFH, PHY, jitter, or underrun value is fabricated.

## Task 4 — Host telemetry parser ✅

**Files**
- `tools/pr1_telemetry_parse.py`
- `tests/test_telemetry_parser.py`

**TDD evidence**
- RED 1: parser tests were added before implementation and failed with import error.
- GREEN 1: parser/CLI implemented.
- Review RED 2: tests reproduced two robustness defects: prefix-only `PR1T`/`PR1E` records were ignored and nonexistent input paths leaked a traceback.
- GREEN 2: exact prefix handling and file-open error boundaries were fixed.

**Parser contract**
- unrelated serial lines => `None`
- valid telemetry => structured dict
- valid event => structured dict with sequence
- unknown extra tokens ignored
- unknown field/event names preserved
- malformed PR1-prefixed records raise `ValueError`
- CLI missing file/malformed record => controlled nonzero exit, no traceback
- JSONL default
- CSV columns: `kind,version,t_us,seq,field,event,value`

## Task 5 — CI and roadmap integration ✅ pending merge

**Files / project records**
- `.github/workflows/pr1-runtime-safe-build.yml`
- GitHub issue #24
- GitHub issue #36

**Implemented**
- [x] parser test job added to safe-runtime workflow
- [x] `tools/**` changes trigger safe-runtime workflow
- [x] issue #24 rewritten to distinguish existing common primitives from unverified physical runtime wiring
- [x] issue #36 marks PR #38 / runtime foundation complete and defines current telemetry boundary
- [x] PR #39 opened against `main`
- [ ] PR #39 merged only after latest-head PR CI is fully green
- [ ] post-merge `main` CI re-verified

## PR #39 review hardening
During final verification the PR head changed with a documentation correction that removed unobserved queue/ARQ values from the safe example. Verification was intentionally restarted instead of relying on earlier green CI.

A follow-up TDD check confirmed the implemented model already uses explicit `OptionalMetric` availability for `max_queue_depth` and `arq_retransmit_sent`. Tests were aligned to verify both sides:
- unavailable optional metrics are omitted even if legacy counters contain nonzero values;
- explicitly observed optional metrics are emitted with their observed values.

This preserves the core rule: **capability/support does not manufacture observation data.**

## Required final verification before merge
The exact PR head must pass fresh PR-triggered:

```text
PR1-DART host tests
PR1 packet simulator
PR1 SX1280 safe runtime build
  - C++ host + ASan/UBSan
  - Python telemetry parser tests
  - ESP32-S3 PlatformIO safe build
```

Full diff review must confirm there is no production dependency on:

```text
Bluetooth
RFCOMM
BlueZ
SHOKZ command bytes/model IDs
RadioLib
PR1_RF_ENABLED=1
SX1280 runtime initialization/transmit/receive
```

## After merge
Round 3 remains only partially complete. The following stay gated behind real RF runtime + exact hardware verification:
- TX enqueue/start/done wiring
- RX DIO/ISR/SPI/re-arm wiring
- actual queue/RSSI/CRC timing collection
- TX gap sweep at 500/300/250/225/200/175/150/125 us
- processing-vs-RF bottleneck classification

Recovery/adaptive layers remain disabled until that classification is measured.
