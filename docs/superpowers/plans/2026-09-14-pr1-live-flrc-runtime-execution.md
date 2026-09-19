# Superpowers execution record — PR1 live fixed-channel FLRC runtime

Plan: `docs/superpowers/plans/2026-09-14-pr1-live-flrc-runtime.md`
Spec: `docs/PR1_DART_IMPLEMENTATION.md`
Branch: `codex/pr1-live-flrc-runtime-20260914`
PR: #45
Base at plan start: `b20bcc957e98bef8e4eb0bc8b0b4e2845f85ba4b`

This file is the persistent execution ledger for the GitHub-connector environment, where the local `.superpowers/sdd/.../progress.md` workspace described by `executing-plans` is not available.

## Pre-flight

- Task 1 profile contract feeds Tasks 3–5: aligned after the `tx_gap_us` ruling below.
- Task 2 telemetry accumulator feeds Task 3 and Task 5: aligned; observation-aware fields remain the host-visible contract.
- Task 3 `RadioPort` feeds Task 4: aligned after removing unsupported FLRC SNR from the port.
- Task 4 RF compile profiles feed Tasks 5–6: aligned; `safe` remains the default environment.
- Task 5 live telemetry feeds Task 6 acceptance logging: aligned as pull-based `PR1T`; event tracing remains in memory to avoid measurement perturbation.

## Task status

- Task 1: complete — explicit Safe/TX/RX compile-time roles, RF gate, fixed FLRC profile, 116-byte packet ceiling assertions.
- Task 2: complete — fixed-storage live telemetry with RSSI, CRC outcome, missing, queue, scheduler and RX timing windows; added `spi_duration_us` as additive field `0x15`.
- Task 3: complete — host-testable `RadioPort` + `FixedLinkRuntime`; TX success-commit semantics, duplicate/gap/malformed/CRC handling, re-arm path and zero-gap stress behavior covered by host tests.
- Task 4: complete — gated SX1280/RadioLib adapter, official LILYGO pin map, RadioLib 7.7.1, safe/TX/RX compile gates.
- Task 5: complete — safe path preserved behind compile-time RF gate; live profile metadata and pull-based `PR1T` snapshots wired into Arduino setup/loop.
- Task 6: complete in software/CI scope — host/sanitizer/parser tests and safe/TX/RX PlatformIO builds are CI-gated. Physical board evidence remains intentionally pending.

## Rulings

1. **Ruling: `tx_period_us` → `tx_gap_us`.** Historical PR1 V4 `TX_GAP_US` meant idle time after a blocking TX completed, not packet start-to-start period. The first implementation used a period and the new timing test was made RED; runtime scheduling was then changed to `tx_done + tx_gap_us` and the suite returned GREEN. `0 us` remains valid for the historical stress point. Cost if wrong: the physical boundary sweep would not be comparable with the earlier PR1 experiment.

2. **Ruling: default TX gap is 5000 us, not 10000 us.** The conservative bring-up profile keeps the historical 5 ms baseline, while the boundary sweep is compiled explicitly through `PR1_TX_GAP_US`. Cost if wrong: only the initial bring-up cadence changes; sweep values are explicit and unaffected.

3. **Ruling: remove FLRC `snrDb()` instead of returning zero.** SX1280 FLRC does not expose the LoRa-style SNR measurement used by the generic RadioLib surface. A dummy zero would violate the project rule “unobserved is not zero.” Cost if wrong: a future verified FLRC SNR source would require re-adding a real optional measurement.

4. **Ruling: no continuous `PR1E` serial streaming in this gate.** The trace ring records events in memory; live serial output is pull-based `PR1T` only because continuous 115200-bps logging can itself create RX-processing stalls and contaminate IRQ/SPI/re-arm timing. Cost if wrong: per-event host chronology is deferred until a non-perturbing export path is added.

5. **Ruling: compile-time branching in `setup()`/`loop()` satisfies the safe/live separation without forcing helper names `setupSafeRuntime()`/`setupLiveRuntime()`.** The behavioral boundary is the requirement; the names in the original plan were an implementation suggestion. Cost if wrong: a later refactor may extract those helpers without changing runtime behavior.

6. **Ruling: do not activate AFH/FEC/ARQ/PHY/controller yet.** `docs/PR1_DART_IMPLEMENTATION.md` requires fixed-channel measurement and loss-source classification before deterministic hopping. Cost if wrong: enabling recovery/adaptation early could hide the receiver-processing bottleneck the current gate exists to measure.

## TDD evidence

- Runtime role/profile contract was introduced test-first; the first contract commit intentionally failed until `pr1_live_profile.hpp` existed.
- Live metrics contract was introduced before `LiveMetrics` implementation.
- Fixed-link TX/RX behavior was introduced with fake-radio tests before the runtime core.
- A zero-period rejection test was deliberately RED, then withdrawn after root-cause investigation showed the historical protocol intentionally includes `0 us` post-TX gap; replacement tests assert post-TX gap semantics and `0 us` support.
- Host tests cover TX sequence commit only after successful physical transmit, RX duplicate/gap handling, malformed packet safety, CRC-failure accounting and RX re-arm.

## Fresh verification evidence

Verified head before this ledger-only commit: `d1e18b4ae048da661140926107424d8038ad69de`.

GitHub Actions workflow `PR1 SX1280 runtime build gates`, run `34767614252`, completed successfully with all of these steps green:

- C++ host and sanitizer regression tests
- Python telemetry parser tests
- RF-disabled `safe` PlatformIO build
- gated fixed-FLRC TX PlatformIO compile
- gated fixed-FLRC RX PlatformIO compile

Other checks on the same head (`host`, `packet-round-trip`) also completed successfully.

## Physical validation boundary

No software/CI result in this PR is evidence of actual RF performance. Before activation step 5 (deterministic static-map hopping), use two physical boards and capture:

1. safe boot metadata;
2. RX live-ready boot;
3. TX live-ready boot;
4. 100-packet sanity;
5. at least 1,000 fixed-link packets;
6. post-TX gap sweep `500 / 300 / 250 / 225 / 200 / 175 / 150 / 125 us` (optional explicit `0 us` stress point);
7. RSSI, good/bad packet outcomes, missing sequences, IRQ→SPI, SPI duration, RX processing, RX re-arm, queue depth and scheduler misses.

Only after those measurements classify the loss source should static all-channel AFH be activated.
