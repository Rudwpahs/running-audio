# PR1 Algorithm Safety Hardening Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Remove the confirmed long-session identity/deadline defects and pre-activation controller/telemetry safety hazards without changing the 16-byte PR1-DART wire header or enabling live RF.

**Architecture:** Keep `sequence:uint16` on wire, but introduce a fixed-size `pr1::sequence::SequenceUnwrapper` that maps wire sequence values onto a monotonic `uint64_t` logical frame timeline. Jitter, ARQ, FEC freshness and AFH activation consume that logical identity; the controller gains an explicit processing-limited path; telemetry only emits measurements whose runtime owner marks them observed.

**Tech Stack:** C++17 fixed-size firmware-common headers, host `assert` tests, GCC `-Wall -Wextra -Werror -pedantic`, ASan/UBSan, Python 3.12 unittest, PlatformIO safe ESP32-S3 build.

**Spec:** `docs/superpowers/specs/2026-09-06-pr1-algorithm-safety-hardening-design.md`

## Global Constraints

- RF application header remains exactly 16 bytes and PR1-DART packet remains 116 bytes.
- `sequence:uint16` remains on wire; do not expand the RF packet for this hardening.
- Common firmware hot paths remain fixed-size and allocation-free; no `std::vector`, `std::map`, exceptions or RTTI.
- Common-layer absolute deadlines become `uint64_t` microseconds; do not base multi-hour deadlines on a permanent 32-bit timestamp.
- Exact half-range sequence delta `0x8000` is ambiguous and must never advance sequence state.
- New-session reset clears sequence, jitter, ARQ, FEC and pending-AFH state owned by the caller/runtime.
- Adaptive controller features default OFF. No hardware timing threshold is claimed physically validated in this plan.
- RF-disabled runtime remains RF-disabled.
- Security/session authentication remains a separate blocker; local `session_generation` is not a cryptographic wire identity.

---

### Task 1: Shared logical sequence identity

**Files:**
- Create: `firmware/common/pr1_sequence.hpp`
- Create: `tests/test_sequence.cpp`

**Interfaces:**
- Produces: `using LogicalFrameIndex = std::uint64_t`
- Produces: `struct LogicalFrameId { std::uint32_t session_generation; LogicalFrameIndex index; }`
- Produces: `enum class UnwrapStatus { Ok, AmbiguousHalfRange, BeforeOrigin, Uninitialized }`
- Produces: `struct UnwrapResult { UnwrapStatus status; LogicalFrameIndex index; bool forward; }`
- Produces: `class SequenceUnwrapper` with `reset(raw, logical)`, `preview(raw)`, `acceptForward(result)`, `latest()`, `rawReference()`, `initialized()`.

- [ ] **Step 1: Write RED tests for wrap and ambiguity**

```cpp
pr1::sequence::SequenceUnwrapper u;
assert(u.preview(1).status == UnwrapStatus::Uninitialized);
u.reset(65534, 65534);
auto a = u.preview(65535); assert(a.status == UnwrapStatus::Ok && a.index == 65535 && a.forward);
assert(u.acceptForward(a));
auto b = u.preview(0); assert(b.status == UnwrapStatus::Ok && b.index == 65536 && b.forward);
assert(u.acceptForward(b));
auto old = u.preview(65535); assert(old.status == UnwrapStatus::Ok && old.index == 65535 && !old.forward);
auto half = u.preview(32768); assert(half.status == UnwrapStatus::AmbiguousHalfRange);
```

Also loop over at least four complete 16-bit wraps numerically and assert `latest()` remains monotonic.

- [ ] **Step 2: Run only sequence test and verify RED**

Run: `g++ -std=c++17 -Wall -Wextra -Werror -pedantic tests/test_sequence.cpp -o /tmp/test_sequence && /tmp/test_sequence`
Expected: FAIL because `pr1_sequence.hpp`/interfaces do not exist.

- [ ] **Step 3: Implement minimal fixed-size unwrapper**

Core nearest-candidate logic:

```cpp
const std::uint16_t modular = static_cast<std::uint16_t>(raw - raw_reference_);
if (modular == 0x8000U) return {UnwrapStatus::AmbiguousHalfRange, 0, false};
const std::int32_t delta = modular <= 0x7FFFU
    ? static_cast<std::int32_t>(modular)
    : -static_cast<std::int32_t>(0x10000U - modular);
if (delta < 0 && static_cast<std::uint64_t>(-static_cast<std::int64_t>(delta)) > latest_) {
  return {UnwrapStatus::BeforeOrigin, 0, false};
}
const auto candidate = delta >= 0
    ? latest_ + static_cast<std::uint64_t>(delta)
    : latest_ - static_cast<std::uint64_t>(-static_cast<std::int64_t>(delta));
return {UnwrapStatus::Ok, candidate, delta > 0};
```

`preview()` is pure. `acceptForward()` mutates only for a valid result whose index is greater than the current logical reference. Reordered/duplicate candidates never move the reference.

- [ ] **Step 4: Run sequence test and full host suite**

Run: `bash tests/run_host_tests.sh`
Expected: all C++ tests including new `test_sequence` pass under normal and ASan/UBSan builds.

- [ ] **Step 5: Commit Task 1**

```bash
git add firmware/common/pr1_sequence.hpp tests/test_sequence.cpp
git commit -m "fix: add long-session logical frame identity"
```

---

### Task 2: Jitter + ARQ long-session correctness

**Files:**
- Modify: `firmware/common/pr1_jitter.hpp`
- Modify: `firmware/common/pr1_arq.hpp`
- Modify: `tests/test_jitter.cpp`
- Modify: `tests/test_arq.cpp`
- Modify: `tests/test_arq_integration.cpp`

**Interfaces:**
- Consumes: `pr1::sequence::LogicalFrameId`, `LogicalFrameIndex`.
- Jitter produces: logical-frame `setAnchor(LogicalFrameId, uint64_t anchor_playout_us, uint64_t target_us)`, `deadlineFor(LogicalFrameId) -> uint64_t`, logical `insert/take`.
- ARQ produces: `RepairRequest::frame_id`, `now_us:uint64_t`, `playout_deadline_us:uint64_t`; tracker states `Free/Reserved/Committed`; `reserve(id)`, `release(id)`, `commit(id)`.
- ARQ produces: eligibility/reservation no longer increments `Stats::sent`; explicit successful commit does.

- [ ] **Step 1: Add RED jitter regression tests**

Use an anchor at logical frame 0 and assert deadlines at frame `32767`, `32768`, `65536`, `90000` (15 min), and `360000` (1 h) equal `anchor + frame*10000ULL`. Insert a delayed logical frame from the prior wrap after current frames and assert it is stale/duplicate by logical identity, not accepted because its low 16 bits match.

- [ ] **Step 2: Add RED ARQ wrap/commit tests**

```cpp
RetransmissionTracker<> tracker;
LogicalFrameId first{1, 1234};
LogicalFrameId wrapped{1, 1234 + 65536ULL};
assert(tracker.reserve(first));
assert(tracker.commit(first));
assert(!tracker.reserve(first));
assert(tracker.reserve(wrapped));
tracker.release(wrapped);
assert(tracker.reserve(wrapped));
```

Create an eligible repair decision, reserve it, simulate enqueue failure with `release`, and assert `stats.sent == 0`; then reserve again, commit successful TX and assert `stats.sent == 1`; a second commit/reserve of the same logical frame is rejected.

- [ ] **Step 3: Run jitter/ARQ tests and verify RED**

Run compiled `test_jitter.cpp`, `test_arq.cpp`, and `test_arq_integration.cpp` individually.
Expected: failures demonstrate old 32768 deadline collapse, raw-`uint16` tracker collision, and premature sent accounting.

- [ ] **Step 4: Implement logical jitter frames and 64-bit deadlines**

Replace frame identity/time members conceptually with:

```cpp
struct Frame {
  bool valid = false;
  sequence::LogicalFrameId id{};
  std::uint16_t wire_sequence = 0;
  std::uint64_t arrival_us = 0;
  std::uint64_t deadline_us = 0;
  ...
};
```

Use checked/saturating `uint64_t` multiplication/addition for deadline computation. Duplicate matching uses the complete logical ID.

- [ ] **Step 5: Implement ARQ reservation/commit state machine**

Each fixed-capacity tracker entry stores complete logical ID and state. `evaluateRepair()` remains a pure eligibility decision; reservation prevents concurrent duplicate scheduling; queue/TX failure calls `release`; successful TX calls `commit` and only then increments `Stats::sent`.

Keep feedback wire encoding exactly 10 bytes. Map the feedback raw sequence into the current logical TX window before constructing a `RepairRequest`.

- [ ] **Step 6: Run Task 2 tests + full sanitizer suite**

Run: `bash tests/run_host_tests.sh`
Expected: long-session jitter, wrap-aware ARQ and existing deadline/map/airtime gates all pass with ASan/UBSan.

- [ ] **Step 7: Commit Task 2**

```bash
git add firmware/common/pr1_jitter.hpp firmware/common/pr1_arq.hpp tests/test_jitter.cpp tests/test_arq.cpp tests/test_arq_integration.cpp
git commit -m "fix: make jitter and ARQ wrap-safe"
```

---

### Task 3: FEC freshness + AFH logical activation

**Files:**
- Modify: `firmware/common/pr1_fec.hpp`
- Modify: `firmware/common/pr1_afh.hpp`
- Modify: `tests/test_fec.cpp`
- Modify: `tests/test_afh.cpp`
- Modify: `tests/test_integration.cpp`

**Interfaces:**
- FEC adds a logical expected-group context; low 16-bit `group_id` remains only a wire consistency check.
- AFH `channelForSequence`/pending activation use `LogicalFrameIndex`; pending activation stores 64-bit logical frame.

- [ ] **Step 1: Write RED FEC stale-wrap test**

Create parity for logical group `7`, then present it as candidate for logical group `7 + 65536`. Even though `group_id & 0xFFFF` matches, recovery must return `InvalidMetadata` unless the expected logical group matches.

- [ ] **Step 2: Write RED AFH >1h and activation-cross-wrap tests**

Run deterministic channel selection at logical frames above `65535`, `262144`, and `360000`; two schedulers with the same config must match. Stage a map activation immediately across the first raw-sequence wrap and assert activation occurs at the logical frame, not according to 16-bit audio wrap.

- [ ] **Step 3: Run FEC/AFH tests and verify RED**

Expected: old FEC only compares `uint16 group_id`, and old AFH API/pending activation lacks the shared logical identity contract.

- [ ] **Step 4: Implement logical FEC consistency and AFH timeline**

Do not expand parity payload. Add logical-group context to local recovery APIs. Change AFH sequence/activation types to `sequence::LogicalFrameIndex`; keep map-version wrap handling separate from audio frame identity.

- [ ] **Step 5: Run affected + full host tests**

Run: `bash tests/run_host_tests.sh`
Expected: all FEC/AFH/integration tests pass under sanitizers.

- [ ] **Step 6: Commit Task 3**

```bash
git add firmware/common/pr1_fec.hpp firmware/common/pr1_afh.hpp tests/test_fec.cpp tests/test_afh.cpp tests/test_integration.cpp
git commit -m "fix: bind FEC and AFH to logical frame timeline"
```

---

### Task 4: Controller processing-overload safety

**Files:**
- Modify: `firmware/common/pr1_link_controller.hpp`
- Modify: `tests/test_link_controller.cpp`
- Modify: `tests/test_integration.cpp`

**Interfaces:**
- `State` adds `ProcessingLimited`.
- `FeatureFlags` defaults every adaptive field to `false`.
- `Metrics` adds explicit observation flags for processing inputs: `processing_metrics_valid`, and concrete queue/timing/scheduler values used by classification.
- `Config` contains provisional software thresholds such as `processing_queue_depth_trigger`, `processing_irq_to_spi_us_trigger`, `processing_rx_us_trigger`, `processing_rearm_us_trigger`; values are configuration only, not hardware claims.
- `Actions` in `ProcessingLimited` keeps fast baseline PHY and disables optional recovery/probe load.

- [ ] **Step 1: Write RED defaults/overload/hysteresis tests**

Assert default actions do not enable adaptive map, ARQ, FEC, probing or adaptive PHY. Feed high PER/burst plus valid queue/timing overload and assert `ProcessingLimited` wins over `Burst`. Assert its PHY remains `Flrc1300Cr34`, FEC/ARQ/probing are false. Feed identical numeric timing values with `processing_metrics_valid=false` and assert overload is not inferred.

Set `good_per_permille` to a non-default value and prove recovery uses it. Assert jitter target changes only when `adaptive_jitter=true`.

- [ ] **Step 2: Run controller tests and verify RED**

Expected: current defaults are ON, no `ProcessingLimited` exists, `good_per_permille` is not used, and jitter targets change without the flag.

- [ ] **Step 3: Implement classifier/action changes minimally**

Classification order:

```text
if valid processing overload -> ProcessingLimited
else if burst/severe PER -> Burst
else if interference -> Interference
else if weak link -> WeakLink
else -> Good
```

Recovery-to-Good requires `per_1s_permille <= good_per_permille` for the hold period. Any jitter target other than baseline 40 ms is gated by `adaptive_jitter`.

- [ ] **Step 4: Run controller/integration and full sanitizer suite**

Run: `bash tests/run_host_tests.sh`
Expected: all tests green; no adaptive action becomes enabled merely because the common module exists.

- [ ] **Step 5: Commit Task 4**

```bash
git add firmware/common/pr1_link_controller.hpp tests/test_link_controller.cpp tests/test_integration.cpp
git commit -m "fix: distinguish receiver overload from RF degradation"
```

---

### Task 5: Telemetry truth + validation/CI gate

**Files:**
- Modify: `firmware/common/pr1_telemetry.hpp`
- Modify: `firmware/t3s3_sx1280_runtime/include/pr1_safe_telemetry.hpp`
- Modify: `firmware/t3s3_sx1280_runtime/README.md`
- Modify: `tests/test_telemetry.cpp`
- Modify: `tests/test_runtime_config.cpp` if output assertions require it
- Modify: `docs/PR1_DART_VALIDATION_MATRIX.md`
- Modify: `docs/PR1_DART_IMPLEMENTATION.md`
- Modify: `.github/workflows/pr1-dart-host-tests.yml` only if a dedicated soak command is added outside `run_host_tests.sh`

**Interfaces:**
- RF-derived snapshot counters become availability-aware values; safe runtime emits no CRC/missing observation.
- Observed zero is still emitted when `available=true`.
- Validation matrix gains wrap/soak/restart/multi-session gates.

- [ ] **Step 1: Write RED telemetry truth tests**

Assert `makeSafeTelemetrySnapshot()` emits state/trace/capability but no `crc_good`, `crc_bad`, or `missing`. Create a live-style snapshot with each metric `available=true,value=0` and assert zero is emitted; then test a non-zero value.

- [ ] **Step 2: Run telemetry tests and verify RED**

Expected: current emitter always outputs CRC/missing counters as zero.

- [ ] **Step 3: Implement observation-aware RF counters**

Use the same `OptionalMetric` contract already used for RSSI/queue/timing. Do not infer availability from the numeric counter value.

- [ ] **Step 4: Update safe runtime README and validation matrix**

Remove safe-mode examples that present unobserved RF zeroes. Add mandatory software gates:
- +32768 regression
- first and multiple sequence wraps
- >15 min and >1 h logical soak
- session reset/stale prior-session rejection
- ARQ enqueue/TX failure rollback
- controller processing-overload classification
- safe telemetry observed-vs-zero distinction

Keep `pr1_dart_sim.py` explicitly labeled regression-only, not RF-performance evidence.

- [ ] **Step 5: Run complete local verification**

Run:

```bash
bash tests/run_host_tests.sh
python -m unittest tests/test_telemetry_parser.py -v
python tools/audio_packet_sim.py --packets 10000
python tools/pr1_dart_sim.py --frames 20000 --seed 12345
python tools/pr1_dart_sim.py --arq-ab --frames 20000 --seed 12345
pio run --project-dir firmware/t3s3_sx1280_runtime -e safe
```

Expected: every command exits 0. Simulator results are structural regression evidence only.

- [ ] **Step 6: Commit Task 5**

```bash
git add firmware/common/pr1_telemetry.hpp firmware/t3s3_sx1280_runtime/include/pr1_safe_telemetry.hpp firmware/t3s3_sx1280_runtime/README.md tests/test_telemetry.cpp tests/test_runtime_config.cpp docs/PR1_DART_VALIDATION_MATRIX.md docs/PR1_DART_IMPLEMENTATION.md .github/workflows/pr1-dart-host-tests.yml
git commit -m "fix: enforce observed telemetry and long-session gates"
```

---

### Task 6: Final review, issue linkage and PR

**Files:**
- No production-code changes unless review finds a demonstrated defect; any defect fix gets its own RED test first.

**Interfaces:**
- Consumes all Task 1-5 deliverables.
- Produces a reviewable PR against `main` and updates #40/#41 with exact evidence.

- [ ] **Step 1: Compare branch against main**

Run/inspect `main...codex/pr1-algorithm-safety-hardening-20260906` and verify wire-size constants and RF compile gate are unchanged.

- [ ] **Step 2: Run verification-before-completion suite fresh**

Re-run every command from Task 5 Step 5 after the final code state. Record exact pass counts/output; do not reuse earlier green results.

- [ ] **Step 3: Perform code review**

Review specifically for raw `uint16 sequence` used as long-lived identity, 32-bit absolute deadlines, premature ARQ sent accounting, adaptive default-ON behavior, unobserved telemetry zeros, and accidental RF enablement.

- [ ] **Step 4: Open PR**

PR body must state:
- fixes #40;
- advances #41 P0/P1 pre-activation blockers;
- does not claim live RF/audio success;
- wire header remains 16 B / packet 116 B;
- exact local verification evidence.

- [ ] **Step 5: Check PR CI and review findings**

All required host/sanitizer/safe-runtime workflows must be green. Any new finding is handled using systematic-debugging + RED test before declaring completion.
