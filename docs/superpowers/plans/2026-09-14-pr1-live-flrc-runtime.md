# PR1 Live Fixed-Channel FLRC Runtime Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Connect the already host-tested PR1-DART packet/instrumentation primitives to a real T3-S3/SX1280 fixed-channel FLRC runtime while preserving the default RF-disabled safe build and keeping AFH/FEC/ARQ/PHY/controller disabled.

**Architecture:** Keep `firmware/common/` as transport-independent logic. Add a small host-testable fixed-link runtime core that consumes a narrow radio interface, then provide one Arduino/RadioLib SX1280 adapter behind `PR1_RF_ENABLED=1`. The interrupt handler only raises a flag/timestamp; SPI reads, packet validation, sequence accounting, telemetry and RX re-arm occur outside the ISR so the existing receiver-processing bottleneck can be measured rather than hidden.

**Tech Stack:** C++17, Arduino ESP32-S3, SX1280, RadioLib FLRC, PlatformIO, existing PR1 telemetry/instrumentation, GitHub Actions.

**Spec:** `docs/PR1_DART_IMPLEMENTATION.md`

## Global Constraints

- Preserve SX1280 FLRC physical payload ceiling at 127 bytes.
- Preserve PR1-DART baseline packet at 116 bytes: 16-byte header + 100-byte codec payload.
- `safe` remains the default PlatformIO environment and must never initialize SPI radio hardware.
- RF-enabled profiles remain non-default and require explicit TX/RX role selection.
- AFH, channel-map adaptation, XOR FEC, deadline ARQ, adaptive PHY and cross-layer controller remain disabled in this phase.
- ISR work is bounded to timestamp/flag capture; no SPI reads, logging, allocation or packet parsing in the ISR.
- No dynamic allocation in the packet hot path.
- Host-visible RF measurements are emitted only after the owning measurement is actually observed; unobserved is not zero.
- First runtime profile uses fixed FLRC 1.3 Mbps / CR 3/4 and one fixed frequency; later PHY/AFH changes must not be smuggled into this phase.

---

### Task 1: Freeze runtime role and fixed-link profile contracts

**Files:**
- Create: `firmware/t3s3_sx1280_runtime/include/pr1_live_profile.hpp`
- Modify: `firmware/t3s3_sx1280_runtime/include/pr1_runtime_config.hpp`
- Modify: `tests/test_runtime_config.cpp`

**Interfaces:**
- Produces: `enum class RuntimeRole : uint8_t { Safe, Tx, Rx }`
- Produces: `struct FixedFlrcProfile { float frequency_mhz; uint16_t bitrate_kbps; uint8_t coding_rate; int8_t output_dbm; uint16_t tx_period_us; }`
- Produces: `constexpr RuntimeRole runtimeRole()` and `constexpr bool liveRfEnabled()`.

- [ ] **Step 1: Write failing host assertions** in `tests/test_runtime_config.cpp` for the new role/profile contract. Assert that the default build resolves to `RuntimeRole::Safe`, `liveRfEnabled()==false`, bitrate is `1300`, coding rate is `3`, and the canonical packet size still fits the 127-byte FLRC limit.
- [ ] **Step 2: Run `tests/run_host_tests.sh` and verify RED** because `pr1_live_profile.hpp`, `RuntimeRole`, and helpers do not exist.
- [ ] **Step 3: Implement the minimal constexpr profile contract.** Keep macro parsing isolated in `pr1_live_profile.hpp`; reject invalid combinations such as `PR1_RF_ENABLED=1` with role `Safe` using `static_assert`/preprocessor checks.
- [ ] **Step 4: Run the full host suite and verify GREEN.**
- [ ] **Step 5: Commit** with `feat: define PR1 fixed FLRC runtime profile`.

### Task 2: Add a host-testable live telemetry accumulator

**Files:**
- Create: `firmware/t3s3_sx1280_runtime/include/pr1_live_metrics.hpp`
- Create: `tests/test_live_metrics.cpp`

**Interfaces:**
- Consumes: `pr1::InstrumentationCounters`, `pr1::telemetry::Snapshot`.
- Produces: `class LiveMetrics` with `onTxQueued`, `onTxStart`, `onTxDone`, `onRxIrq`, `onSpiStart`, `onSpiEnd`, `onRxPacket`, `onRxCrcFail`, `onRxRearmStart`, `onRxRearmDone`, `onSchedulerMiss`, and `snapshot()`.
- Produces availability-aware duration and queue-depth metrics without fabricating zeros before observation.

- [ ] **Step 1: Create `tests/test_live_metrics.cpp`** that starts with a fresh accumulator and verifies RSSI/timing fields are unavailable, then feeds one receive lifecycle and verifies CRC-good, RSSI, IRQ→SPI, SPI duration, RX-processing and RX-rearm fields become available with the expected values.
- [ ] **Step 2: Run host tests and verify RED** because `LiveMetrics` is missing.
- [ ] **Step 3: Implement fixed-storage metrics only.** Use the existing duration-window/telemetry primitives; do not allocate and do not print from this class.
- [ ] **Step 4: Add a second test case** proving an observed zero counter is distinguishable from an unobserved field.
- [ ] **Step 5: Run host tests and sanitizers; verify GREEN.**
- [ ] **Step 6: Commit** with `feat: add live RF telemetry accumulator`.

### Task 3: Build the fixed-link runtime state machine against a radio interface

**Files:**
- Create: `firmware/t3s3_sx1280_runtime/include/pr1_radio_port.hpp`
- Create: `firmware/t3s3_sx1280_runtime/include/pr1_fixed_link_runtime.hpp`
- Create: `tests/test_fixed_link_runtime.cpp`

**Interfaces:**
- Produces: `class RadioPort` abstracting `beginFixedFlrc`, `startReceive`, `readPacket`, `transmit`, `rssiDbm`, `snrDb` and `setPacketReceivedCallback` semantics without Arduino types.
- Produces: `FixedLinkRuntime::tick(now_us)` for TX scheduling and RX event servicing.
- Consumes: canonical PR1 packet codec and `LiveMetrics`.

- [ ] **Step 1: Write a fake `RadioPort` in the test file** and a failing TX test: at the configured period the runtime must serialize exactly one 116-byte canonical packet, increment sequence exactly once after successful transmit, and record TX timing.
- [ ] **Step 2: Run host tests and verify RED.**
- [ ] **Step 3: Implement the minimal TX state machine** with stack/fixed arrays only. A failed transmit must not silently advance the success counter.
- [ ] **Step 4: Write a failing RX test** where the fake radio raises a receive event; verify the runtime reads one packet, validates/decodes it, updates sequence/missing metrics, records RSSI, then re-arms receive.
- [ ] **Step 5: Implement the minimal RX state machine.** Packet parsing and re-arm happen in `tick`, never inside the callback.
- [ ] **Step 6: Add tests for malformed length, duplicate sequence, and a sequence gap.** These must not crash, and the gap must increment missing accounting deterministically.
- [ ] **Step 7: Run full host + sanitizer suite; verify GREEN.**
- [ ] **Step 8: Commit** with `feat: add host-tested fixed FLRC runtime core`.

### Task 4: Add the T3-S3/SX1280 RadioLib adapter behind the RF gate

**Files:**
- Create: `firmware/t3s3_sx1280_runtime/include/pr1_sx1280_radiolib.hpp`
- Create: `firmware/t3s3_sx1280_runtime/src/pr1_sx1280_radiolib.cpp`
- Modify: `firmware/t3s3_sx1280_runtime/platformio.ini`

**Interfaces:**
- Implements: `RadioPort` using `SX1280` + `Module` + board SPI/pin configuration.
- Uses: `beginFLRC(ConfigFLRC_t)`, `setRfSwitchPins(rx_enable, tx_enable)`, interrupt-driven receive, `readData`, `transmit`, `getRSSI`, `getSNR`.

- [ ] **Step 1: Add non-default `rf_tx_compile` and `rf_rx_compile` environments** with `PR1_RF_ENABLED=1`, explicit role macros, and a pinned RadioLib dependency. Keep `default_envs = safe` unchanged.
- [ ] **Step 2: Verify safe build remains independent of RadioLib code paths** by keeping all RadioLib includes/objects behind `#if PR1_RF_ENABLED`.
- [ ] **Step 3: Implement the adapter using the official LILYGO SX1280 pin map and RadioLib FLRC API.** Configure SPI with SCLK=5, MISO=3, MOSI=6, NSS=7, DIO1=9, RST=8, BUSY=36, RX switch=21, TX switch=10 from `pr1_board_config.hpp`; do not duplicate numeric pins in the `.cpp`.
- [ ] **Step 4: Configure FLRC from `FixedFlrcProfile` and reject initialization errors before entering the runtime loop.**
- [ ] **Step 5: Compile `safe`, `rf_tx_compile`, and `rf_rx_compile` in CI or PlatformIO and fix warnings/errors without changing the safe boundary.**
- [ ] **Step 6: Commit** with `feat: add gated SX1280 RadioLib FLRC adapter`.

### Task 5: Wire Arduino setup/loop to safe or live fixed-link mode

**Files:**
- Modify: `firmware/t3s3_sx1280_runtime/src/main.cpp`
- Modify: `firmware/t3s3_sx1280_runtime/README.md`

**Interfaces:**
- Safe build keeps the current metadata + safe telemetry behavior.
- RF TX/RX builds construct the adapter/runtime and emit `PR1T`/`PR1E` records from observed live metrics.

- [ ] **Step 1: Add a compile-contract assertion/test** that the safe branch does not instantiate or reference `Sx1280RadioLibPort`.
- [ ] **Step 2: Refactor `main.cpp` into `setupSafeRuntime()` and `setupLiveRuntime()` selected at compile time.** Preserve current safe output byte-for-byte where practical.
- [ ] **Step 3: In live mode print profile metadata** including role, frequency, bitrate, coding rate, output power and packet bytes before radio initialization.
- [ ] **Step 4: In `loop`, call only `FixedLinkRuntime::tick(micros())` plus bounded telemetry flushing; no `delay()` in the live hot path.
- [ ] **Step 5: Update README with exact build commands and an explicit warning that RF compile success is not hardware performance validation.**
- [ ] **Step 6: Build all three environments and run host tests.**
- [ ] **Step 7: Commit** with `feat: activate fixed-channel PR1 FLRC runtime profiles`.

### Task 6: Add CI compile gates and runtime acceptance logging contract

**Files:**
- Modify: `.github/workflows/pr1-runtime-safe-build.yml`
- Modify: `firmware/t3s3_sx1280_runtime/README.md`
- Modify: `docs/PR1_DART_IMPLEMENTATION.md`

**Interfaces:**
- CI proves host logic + safe build + RF compileability only.
- Physical logs remain the evidence source for IRQ→SPI, SPI duration, RX processing, RX re-arm, PER and scheduler behavior.

- [ ] **Step 1: Extend CI with compile-only RF TX/RX jobs** while preserving the existing safe job.
- [ ] **Step 2: Keep CI language explicit:** no job may label RF compile success as board-verified or field-validated.
- [ ] **Step 3: Document the first board test sequence:** safe boot → fixed RX boot → fixed TX boot → 100-packet sanity → 1,000-packet measurement → TX-gap sweep 500/300/250/225/200/175/150/125 us.
- [ ] **Step 4: Run GitHub Actions and verify host tests, parser tests, simulator and all runtime compile jobs are green.**
- [ ] **Step 5: Commit** with `ci: compile gated PR1 FLRC runtime profiles`.

## Self-review

- Spec coverage: this plan implements only activation steps 1–4 from `PR1_DART_IMPLEMENTATION.md`; it intentionally does not activate AFH/FEC/ARQ/audio/PHY/controller.
- No protocol expansion: packet stays 116 bytes and wire sequence stays uint16_t.
- No hidden adaptive behavior: fixed channel, fixed 1.3 Mbps / CR 3/4 only.
- Hardware claims remain deferred until physical logs exist.
- Follow-up plans after this gate: static deterministic AFH → adaptive channel quality → XOR FEC → deadline ARQ → jitter/Opus/PLC → PHY ladder → cross-layer controller → optional music-oriented PLC research path.
