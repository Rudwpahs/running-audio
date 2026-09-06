# PR1 Control Telemetry Instrumentation Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a PR1-owned transport-neutral telemetry schema, extend RX timing instrumentation, emit deterministic RF-disabled runtime telemetry, and parse it on the host without introducing SHOKZ/Bluetooth dependencies.

**Architecture:** Reuse the existing `pr1_instrumentation.hpp` fixed-size primitives. Add `pr1_telemetry.hpp` as the semantic state/capability/field layer, keep serial line formatting in the safe runtime, and parse those lines with a Python-standard-library host tool. SHOKZ material influences design patterns only and is never linked into PR1 firmware.

**Tech Stack:** C++17 header-only common firmware logic, Arduino/ESP32-S3 PlatformIO safe runtime, Python 3.12 standard library, GitHub Actions.

**Spec:** `docs/superpowers/specs/2026-09-06-pr1-control-telemetry-instrumentation-design.md`

## Global Constraints

- `PR1_RF_ENABLED=0` remains the only runtime profile in this plan.
- No SX1280/RadioLib initialization or transmission is introduced.
- No Bluetooth/RFCOMM/BlueZ/SHOKZ command code is linked into PR1.
- No dynamic allocation or heap-owning containers are added to common telemetry/instrumentation.
- Telemetry schema version is exactly `1`.
- PR1-DART radio payload ceiling remains `127` bytes and baseline packet remains `116` bytes.
- Telemetry is Serial text only in this plan; no per-audio-packet telemetry and no binary RF telemetry.
- Existing C++ normal + ASan/UBSan tests, packet simulator, and PlatformIO safe build must remain green.

---

### Task 1: PR1 Telemetry Schema and Snapshot Contract

**Files:**
- Create: `firmware/common/pr1_telemetry.hpp`
- Create: `tests/test_telemetry.cpp`

**Interfaces:**
- Consumes: `pr1::instrumentation::Counters` and `pr1::instrumentation::Event` from `firmware/common/pr1_instrumentation.hpp`.
- Produces: `pr1::telemetry::DeviceState`, `Capability`, `FieldId`, `OptionalMetric`, `Snapshot`, `FieldValue`, `deviceStateName()`, `fieldName()`, `eventName()`, `forEachSnapshotField()`.

- [ ] **Step 1: Write the failing schema test**

Create `tests/test_telemetry.cpp` with assertions for stable numeric IDs, state/capability names, ordered snapshot emission, and omission of unavailable optional fields:

```cpp
#include <cassert>
#include <cstdint>
#include <iostream>
#include <vector>

#include "../firmware/common/pr1_telemetry.hpp"

int main() {
  using namespace pr1::telemetry;

  static_assert(kTelemetrySchemaVersion == 1);
  static_assert(static_cast<std::uint8_t>(FieldId::DeviceState) == 0x01);
  static_assert(static_cast<std::uint8_t>(FieldId::CapabilityMask) == 0x14);
  static_assert(static_cast<std::uint32_t>(Capability::TimingTrace) == 8u);

  assert(std::string_view{deviceStateName(DeviceState::SafeIdle)} == "safe_idle");
  assert(std::string_view{fieldName(FieldId::CrcBad)} == "crc_bad");

  Snapshot snapshot{};
  snapshot.state = DeviceState::SafeIdle;
  snapshot.capability_mask = capabilityMask(Capability::TimingTrace);
  snapshot.counters.crc_bad = 4;
  snapshot.counters.scheduler_misses = 2;
  snapshot.trace_overwrites = 3;
  snapshot.rssi_dbm = {false, -41};
  snapshot.irq_to_spi_us = {true, 177};

  std::vector<FieldValue> fields;
  forEachSnapshotField(snapshot, [&](FieldValue value) { fields.push_back(value); });

  assert(fields.front().field == FieldId::DeviceState);
  assert(fields.front().value == 1);
  assert(fields.back().field == FieldId::CapabilityMask);

  bool saw_rssi = false;
  bool saw_irq = false;
  bool saw_crc_bad = false;
  for (const auto& field : fields) {
    if (field.field == FieldId::RssiDbm) saw_rssi = true;
    if (field.field == FieldId::IrqToSpiUs && field.value == 177) saw_irq = true;
    if (field.field == FieldId::CrcBad && field.value == 4) saw_crc_bad = true;
  }
  assert(!saw_rssi);
  assert(saw_irq);
  assert(saw_crc_bad);

  std::cout << "test_telemetry: PASS\n";
  return 0;
}
```

Include `<string_view>` in the final test file.

- [ ] **Step 2: Run the new test and verify RED**

Run:

```bash
g++ -std=c++17 -Wall -Wextra -Werror -pedantic -O2 tests/test_telemetry.cpp -o /tmp/test_telemetry
```

Expected: compilation fails because `firmware/common/pr1_telemetry.hpp` does not exist.

- [ ] **Step 3: Implement the minimal telemetry header**

Create `firmware/common/pr1_telemetry.hpp` using only `<cstdint>` and `pr1_instrumentation.hpp`. Implement the exact enums/structs from the spec, plus:

```cpp
constexpr std::uint32_t capabilityMask(Capability capability) {
  return static_cast<std::uint32_t>(capability);
}

constexpr std::uint32_t capabilityMask(Capability first, Capability second) {
  return capabilityMask(first) | capabilityMask(second);
}
```

Implement deterministic C-string name switches for all `DeviceState`, `FieldId`, and current `instrumentation::Event` values. Unknown enum values return `"unknown"`.

Implement `forEachSnapshotField()` in ascending `FieldId` order. Always emit: device state, CRC good, CRC bad, missing, max queue depth, scheduler misses, trace overwrites, ARQ retransmit sent, capability mask. Emit optional fields only when `available` is true.

- [ ] **Step 4: Run the isolated telemetry test and verify GREEN**

Run:

```bash
g++ -std=c++17 -Wall -Wextra -Werror -pedantic -O2 tests/test_telemetry.cpp -o /tmp/test_telemetry && /tmp/test_telemetry
```

Expected output:

```text
test_telemetry: PASS
```

- [ ] **Step 5: Run the full C++ normal/sanitizer suite**

Run:

```bash
bash tests/run_host_tests.sh
```

Expected: every `tests/test_*.cpp` passes under normal and ASan/UBSan builds.

- [ ] **Step 6: Commit Task 1**

Commit message:

```text
feat: add PR1 telemetry schema and snapshot contract
```

---

### Task 2: Extend RX Timing Event Vocabulary

**Files:**
- Modify: `firmware/common/pr1_instrumentation.hpp`
- Modify: `tests/test_instrumentation.cpp`
- Modify: `tests/test_telemetry.cpp`

**Interfaces:**
- Consumes: existing `pr1::instrumentation::Event`, `TraceRing`, `Counters`.
- Produces: new event values `RxPacketOk`, `RxCrcFail`, `RxRearmStart`, `RxRearmDone`, `QueueDepth`, and deterministic event names through `telemetry::eventName()`.

- [ ] **Step 1: Add failing event-name assertions**

Append to `tests/test_telemetry.cpp`:

```cpp
assert(std::string_view{eventName(pr1::instrumentation::Event::RxPacketOk)} == "rx_packet_ok");
assert(std::string_view{eventName(pr1::instrumentation::Event::RxCrcFail)} == "rx_crc_fail");
assert(std::string_view{eventName(pr1::instrumentation::Event::RxRearmStart)} == "rx_rearm_start");
assert(std::string_view{eventName(pr1::instrumentation::Event::RxRearmDone)} == "rx_rearm_done");
assert(std::string_view{eventName(pr1::instrumentation::Event::QueueDepth)} == "queue_depth");
```

Append to `tests/test_instrumentation.cpp` a ring record using `RxRearmDone` with `value=125` and assert it survives retrieval.

- [ ] **Step 2: Run the two tests and verify RED**

Run:

```bash
g++ -std=c++17 -Wall -Wextra -Werror -pedantic -O2 tests/test_instrumentation.cpp -o /tmp/test_instrumentation
g++ -std=c++17 -Wall -Wextra -Werror -pedantic -O2 tests/test_telemetry.cpp -o /tmp/test_telemetry
```

Expected: compilation fails because the new event enum members do not exist.

- [ ] **Step 3: Add only the five new events**

Add after `SpiReadEnd` in `Event`:

```cpp
RxPacketOk,
RxCrcFail,
RxRearmStart,
RxRearmDone,
QueueDepth,
```

Update `eventName()` in `pr1_telemetry.hpp` for those names. Do not add unrelated events.

- [ ] **Step 4: Run full host tests and verify GREEN**

Run:

```bash
bash tests/run_host_tests.sh
```

Expected: all normal and sanitizer tests pass.

- [ ] **Step 5: Commit Task 2**

Commit message:

```text
feat: add RX bottleneck timing trace events
```

---

### Task 3: Safe Runtime Serial Telemetry

**Files:**
- Modify: `firmware/t3s3_sx1280_runtime/src/main.cpp`
- Modify: `tests/test_runtime_config.cpp`

**Interfaces:**
- Consumes: `pr1::telemetry::Snapshot`, `forEachSnapshotField()`, `fieldName()`, `kTelemetrySchemaVersion`.
- Produces: deterministic boot-time `PR1T` lines; RF remains disabled.

- [ ] **Step 1: Add a failing safe-runtime compile contract**

Extend `tests/test_runtime_config.cpp` to include `pr1_telemetry.hpp` and assert:

```cpp
static_assert(pr1::telemetry::kTelemetrySchemaVersion == 1);
static_assert(pr1::telemetry::capabilityMask(pr1::telemetry::Capability::TimingTrace) == 8u);
```

Also assert that a default safe snapshot can be constructed with `DeviceState::SafeIdle` and TimingTrace capability.

- [ ] **Step 2: Run full host tests before runtime modification**

Run:

```bash
bash tests/run_host_tests.sh
```

Expected: GREEN; this confirms the runtime integration starts from a clean host baseline.

- [ ] **Step 3: Add runtime telemetry emission**

In `main.cpp`, include the common telemetry header via the runtime project's include path:

```cpp
#include "../../common/pr1_telemetry.hpp"
```

Add:

```cpp
void printSafeTelemetry() {
  pr1::telemetry::Snapshot snapshot{};
  snapshot.state = pr1::telemetry::DeviceState::SafeIdle;
  snapshot.capability_mask =
      pr1::telemetry::capabilityMask(pr1::telemetry::Capability::TimingTrace);

  const std::uint32_t t_us = micros();
  pr1::telemetry::forEachSnapshotField(snapshot, [&](pr1::telemetry::FieldValue item) {
    Serial.printf("PR1T v=%u t_us=%lu field=%s value=%lld\n",
                  static_cast<unsigned>(pr1::telemetry::kTelemetrySchemaVersion),
                  static_cast<unsigned long>(t_us),
                  pr1::telemetry::fieldName(item.field),
                  static_cast<long long>(item.value));
  });
}
```

Call `printSafeTelemetry()` once after `printBootMetadata()` in `setup()`.

Do not add a periodic loop emission in this plan; one boot snapshot is sufficient and avoids unnecessary serial load during later timing bring-up.

- [ ] **Step 4: Build the safe firmware**

Run:

```bash
pio run --project-dir firmware/t3s3_sx1280_runtime -e safe
```

Expected: successful ESP32-S3 firmware build with `PR1_RF_ENABLED=0`.

- [ ] **Step 5: Run full host tests again**

Run:

```bash
bash tests/run_host_tests.sh
```

Expected: all tests pass.

- [ ] **Step 6: Commit Task 3**

Commit message:

```text
feat: emit deterministic safe runtime telemetry
```

---

### Task 4: Host Telemetry Parser and CLI

**Files:**
- Create: `tools/pr1_telemetry_parse.py`
- Create: `tests/test_telemetry_parser.py`

**Interfaces:**
- Consumes: `PR1T` and `PR1E` line formats from the spec.
- Produces: `parse_line(line: str) -> dict | None`, JSONL CLI output, CSV CLI output.

- [ ] **Step 1: Write failing Python unit tests**

Create `tests/test_telemetry_parser.py` using only `unittest`, `io`, `csv`, `json`, `subprocess`, `sys`, and `pathlib`. Import the parser module by inserting repository `tools/` into `sys.path`.

Required tests:

```python
self.assertEqual(
    parse_line("PR1T v=1 t_us=123 field=crc_bad value=4"),
    {"kind": "telemetry", "version": 1, "t_us": 123, "field": "crc_bad", "value": 4},
)

self.assertEqual(
    parse_line("PR1E v=1 t_us=456 seq=9 event=spi_read_start value=0"),
    {"kind": "event", "version": 1, "t_us": 456, "seq": 9, "event": "spi_read_start", "value": 0},
)

self.assertIsNone(parse_line("PR1_RUNTIME_BOOT"))
self.assertEqual(parse_line("PR1T v=1 t_us=1 field=future_metric value=7 x=ignored")["field"], "future_metric")

with self.assertRaises(ValueError):
    parse_line("PR1T v=x t_us=1 field=crc_bad value=0")

with self.assertRaises(ValueError):
    parse_line("PR1E v=1 t_us=1 event=rx_packet_ok value=0")
```

Add a subprocess CLI test that feeds one telemetry + one event line and asserts CSV header exactly:

```text
kind,version,t_us,seq,field,event,value
```

- [ ] **Step 2: Run tests and verify RED**

Run:

```bash
python -m unittest tests/test_telemetry_parser.py -v
```

Expected: import failure because `tools/pr1_telemetry_parse.py` does not exist.

- [ ] **Step 3: Implement `parse_line()` and CLI**

Implementation rules:

```python
def parse_line(line: str):
    line = line.strip()
    if not line.startswith(("PR1T ", "PR1E ")):
        return None
    tokens = line.split()
    prefix = tokens[0]
    values = {}
    for token in tokens[1:]:
        if "=" not in token:
            raise ValueError(f"malformed token: {token}")
        key, value = token.split("=", 1)
        values[key] = value
```

Then validate required keys by prefix and convert `v`, `t_us`, `seq`, and `value` with `int(text, 10)`. Unknown extra keys are ignored. Empty required strings are rejected.

CLI:

- optional positional input path; absent means stdin;
- `--format jsonl|csv`, default `jsonl`;
- unrelated lines skipped;
- malformed PR1 lines print an error to stderr and exit non-zero;
- CSV uses fixed columns `kind,version,t_us,seq,field,event,value`.

- [ ] **Step 4: Run parser tests and verify GREEN**

Run:

```bash
python -m unittest tests/test_telemetry_parser.py -v
```

Expected: all parser unit tests pass.

- [ ] **Step 5: Run full C++ host tests to protect cross-language changes**

Run:

```bash
bash tests/run_host_tests.sh
```

Expected: all C++ normal/sanitizer tests remain green.

- [ ] **Step 6: Commit Task 4**

Commit message:

```text
feat: add PR1 telemetry host parser
```

---

### Task 5: CI, Roadmap, and Final Verification

**Files:**
- Modify: `.github/workflows/pr1-runtime-safe-build.yml`
- Modify: GitHub issue `#24`
- Modify: GitHub issue `#36`

**Interfaces:**
- Consumes: parser test path and existing safe-build workflow.
- Produces: CI-enforced parser test + updated execution roadmap.

- [ ] **Step 1: Add parser tests to the safe-build workflow**

Insert after Python setup and before PlatformIO install:

```yaml
      - name: Python telemetry parser tests
        run: python -m unittest tests/test_telemetry_parser.py -v
```

Keep PlatformIO pinned at `6.1.18`.

- [ ] **Step 2: Verify workflow YAML content manually**

Confirm the workflow still contains all of:

```text
bash tests/run_host_tests.sh
python -m unittest tests/test_telemetry_parser.py -v
platformio==6.1.18
pio run --project-dir firmware/t3s3_sx1280_runtime -e safe
```

- [ ] **Step 3: Update issue #24**

Rewrite #24 so completed common primitives are marked complete and the remaining runtime work references the stable `PR1T`/`PR1E` schema. Preserve the initial engineering gates. Explicitly state that physical IRQ/SPI timing values remain unverified until RF runtime and board tests exist.

- [ ] **Step 4: Update issue #36**

Mark Round 2 complete via PR #38 / merge SHA `024911d3c3cd0eebc24a5ac60676584917a54720`. Expand Round 3 to include PR1-owned serial telemetry and SHOKZ-reference boundary. Do not mark Round 3 complete until this PR merges.

- [ ] **Step 5: Push-trigger CI and inspect all branch jobs**

Required branch checks:

```text
PR1-DART host tests = success
PR1 packet simulator = success
PR1 SX1280 safe runtime build = success
```

Inside `PR1 SX1280 safe runtime build`, confirm:

```text
C++ host and sanitizer regression tests = success
Python telemetry parser tests = success
Build RF-disabled runtime = success
```

- [ ] **Step 6: Review the full diff against the spec**

Reject the change if any diff introduces:

```text
Bluetooth
RFCOMM
BlueZ
RadioLib
SX1280 begin/transmit/receive calls
PR1_RF_ENABLED=1
SHOKZ command bytes
heap-owning containers in firmware/common
```

Test-only use of `std::vector` in `tests/test_telemetry.cpp` is allowed.

- [ ] **Step 7: Open a PR against `main`**

PR title:

```text
PR1: add transport-neutral telemetry and safe runtime instrumentation
```

PR body must state:

- SHOKZ source bundle influenced schema design only;
- no SHOKZ/Bluetooth code is linked;
- RF remains disabled;
- physical timing measurements are not claimed;
- branch CI is green;
- relates to #24 and #36.

- [ ] **Step 8: Do not merge until fresh PR CI is green**

After PR creation, verify the PR-triggered versions of all required jobs. Merge only with the exact verified head SHA.
