# Gate C1 — USB control-plane isolation (issue #52), 2026-10-03

Branch `claude/pr1-dart-layered-board-20261003`; `main` untouched. The boards, placement, PHY (FLRC 1.3 Mbps
CR 3/4, 0 dBm, 116 B) and gap (150 µs) are the same as in Gates A/B/C. Things C1 does **not** change:
- the estimator thresholds, probe policy and map logic (`pr1_channel_quality.hpp` behaviour is identical to Gate C);
- the two-phase commit and the future activation frame (lead 600, guard 100);
- the state of the other layers: FEC, ARQ, adaptive PHY, controller, Opus/jitter/PLC all stay OFF;
- the measurement window, which still has no serial polling.

Gate D, the wireless reverse link, the controller and adaptive PHY were not started.

## Verdict
- **C1 removes the control-adjacent loss cluster.**
  - Same session: losses within 0–3 frames after a control event fell from **145** with the old Gate C image
    to **1** with C1, over 251 vs 261 control events.
  - Pooled loss: **C1 0.353 %**, **old C 0.792 %**, **B1 1.240 %**.
- **Root cause, measured: flash instruction-cache eviction, not USB-CDC CPU time.**
  - The ESP32-S3 runs flash code through **one 16 KB I-cache shared by both cores**.
  - Any rarely-run code evicts the RX post-read path. The next post-read then takes 100–180 µs longer.
    At a 150 µs gap that loses the next frame.
  - Examples of such code: `snprintf` (`_svfprintf_r` alone is ~12 KB), the HWCDC driver and ISR, and the
    map/estimator logic.
- **Fix: control-plane and adaptive-map code no longer runs from flash during a run.** The four parts:
  - core-0 control task;
  - polled USB FIFO access from IRAM;
  - self-contained formatter/parser in IRAM;
  - cold map/estimator functions placed in IRAM.
- The estimator itself is still uncalibrated. That is C2.

## 1. Measured facts

### 1.1 Diagnosis sequence (short 2–3k-packet runs, `smokeC1/`, each image's sha256 in its `metadata.json`)
| step | firmware change | losses 0–3 fr after ctrl | slow RX post-read events* |
|---|---|---|---|
| s1 | USB ISR + parse/format moved to core 0 (still `snprintf`) | 9 / 14 events | (not instrumented) |
| s2 | + TX/RX timing diagnostics | 13 / 18 | (not instrumented) |
| s3 | + `RxSlowReady` (IRQ→re-armed > 1950 µs) | 12 / 21 | 11 |
| s4 | + core-0 USB work only inside an RX window (spin on `micros()`) | 18 / 20 | 13 |
| s5 | + post-read breakdown | 13 / 20 | 13 |
| s6 | window wait via a RAM flag + ROM delay (no timer polling) | 17 / 25 | 11 |
| s7 | + HWCDC USB interrupts masked outside the window | 12 / 18 | 10 |
| k1 | **B1k: Gate B + rarely-run text formatting every 600 frames, no USB, no map** | – | **6 / 6 cold-code runs → 6 slow frames, all lost** |
| k2 | B1k + per-frame "re-warm" via SPI GetPacketStatus (rejected) | – | 6 / 6 still slow; **CRC 15 → 122** |
| s8 | **final C1** (IRAM + polled FIFO + custom formatter) | **0 / 18** | **0** |

\* Slow RX post-read event = IRQ → RX re-armed > 1950 µs (normal ≈ 1880 µs), recorded from the idle loop.

How the slow post-read was localised:
- **Breakdown (s5):** IRQ→SPI start stayed at 3–10 µs, so this is not idle work overlapping a reception.
  The SPI read took 1240 → 1280–1330 µs, decode/follow 230 → 240–300 µs, and re-arm stayed at ~425 µs.
- **k1 shows the mechanism with nothing else involved:** no USB traffic, no adaptive map; only cold code.
  All three stages slowed (SPI +80, processing +50, re-arm +30 µs).
- **k2 rules out SPI activity as a fix:**
  - The extra SPI transactions made the slow frames only partially better.
  - They also showed that **any SPI activity during a reception sharply increases CRC errors**: 15 → 122.

### 1.2 C1 implementation
1. **Core-0 control task.** It calls `Serial.begin()`, so HWCDC's interrupt is allocated on core 0.
   - After the boot banner it masks HWCDC's IN/OUT interrupts.
   - It reads and writes the USB-Serial/JTAG FIFOs by polling with `hal/usb_serial_jtag_ll.h`, from IRAM.
   - HWCDC is used again only for post-run pull dumps.
2. **RX-only window.** Core 0 touches USB only while the RX idle loop publishes "open".
   - Open means: from RX re-arm until 600 µs before the next expected RX-done.
   - Host OUT data waits in the 64-byte hardware FIFO meanwhile.
3. **Record queues.** Core 1 exchanges only fixed-size records with core 0, through lock-free SPSC queues.
   - Core 1 does no parsing, no printf and no Serial work during a run.
4. **Self-contained parser/formatter in IRAM.**
   - The output is byte-identical to the Gate C `snprintf` text: `tests/test_ctrl_plane.cpp` compares 20 000
     random records against the old formats.
   - The key rules match the old `strstr`/`strtoull` reader for host-generated lines.
5. **IRAM placement.** Cold adaptive-map, estimator and control functions carry `PR1_IRAM`
   (`firmware/common/pr1_placement.hpp`).
   - Linking this needs `build_src_flags = -mtext-section-literals`, which applies to project sources only.
   - This changes placement only, not logic: the host tests (`test_channel_quality`, `test_afh`,
     `test_fixed_link_runtime`, `test_ctrl_plane`) pass.
6. **Diagnostics.** All are recorded from the idle loop except:
   - TX: a compare and a store in the TX hot path;
   - RX: a few stores after `startReceive()`.

   The events are `TxLate`, `TxSlow`, `RxLate`, `RxSlowReady`, `RxSlowBreakdown` and `IdleOverlap`.
   `PR1_DIAG_COLD_WORK_EVERY` (default 0) is the k1 cache test.
7. **B1.** B1 = Gate B with the same C1 infrastructure (items 1–3, 5 and 6 compiled in). B1 and C1 differ only
   in `PR1_ENABLE_ADAPTIVE_MAP`.

### 1.3 Same-session interleave B1 → C1 → C(old) ×3, 150 µs / 10k (`gateC1-interleave/`, `stats.json`)
"C(old)" is the unmodified Gate C image (rx sha 96cc5a1a…, tx d4fcb928…), identical to `gateC-interleave/`.

| run | loss | CRC | no-IRQ fr | ctrl ev | lost ≤3 fr after ctrl | slow post-read | map v / min active | IRQ→ready p99 |
|---|---|---|---|---|---|---|---|---|
| B1 i1 | 0.951 % | 58 | 47 | 0 | – | 0 | – | 1906 |
| C1 i2 | **0.296 %** | 18 | 15 | 90 | **0** | **0** | 10 / 21 | 1889 |
| C i3 | 0.736 % | 19 | 63 | 78 | 45 | n/a | 13 / 12 | 1880 |
| B1 i4 | 1.367 % | 88 | ≥63† | 0 | – | 0 | – | 1904 |
| C1 i5 | **0.367 %** | 23 | 18 | 92 | **0** | **0** | 12 / 19 | 1888 |
| C i6 | 0.860 % | 28 | 68 | 87 | 50 | n/a | 9 / 12 | 1886 |
| B1 i7 | 1.403 % | 82 | ≥72† | 0 | – | 0 | – | 1902 |
| C1 i8 | **0.395 %** | 22 | 22 | 79 | **1** | **0** | 16 / 14 | 1883 |
| C i9 | 0.779 % | 18 | 69 | 86 | 50 | n/a | 10 / 12 | 1984 |

† Lower bound: the 160-entry anomaly list overflowed. The loss totals come from the telemetry counter and are complete.

Pooled:
- **B1** 411/33139 = **1.240 %**; **C1** 118/33455 = **0.353 %**; **C(old)** 265/33470 = **0.792 %**.
- **C1 vs B1:** −0.888 pp, 95 % CI [−1.023, −0.752], z = −12.9, **relative −71.6 %**.
  - CRC −72.6 % (z = −9.8); no-IRQ frames −70.1 % (z = −8.3).
- **C(old) vs B1:** loss −36.2 % (z = −5.8).
  - CRC −71.8 %. No-IRQ +8.8 % (z = 0.8): channel avoidance removed CRC loss, but the cache-induced no-IRQ
    losses gave much of the gain back.
- **C1 vs C(old):** loss −55.5 % (z = −7.5).
  - No-IRQ −72.5 % (z = −9.1); CRC unchanged (−3 %, z = −0.2).
  - So C1's gain over C comes entirely from removing the control-plane-induced no-IRQ losses.

Control handling time and timing (C1 runs):
- RX core 1: p50 5, p95 6–7, p99 ≤ 13 µs. TX core 1: p50 3–4, p99 ≤ 11 µs.
  For comparison, the old C was RX p99 184 µs and TX p99 261–266 µs.
- Core-0 USB work burst: max 40 µs. The relay never starved: proposals = commits in every run, and
  `in_dropped` = `parse_fail` = 0.
- Scheduler misses: C1 0, B1 1, C(old) 1.
- SPI p99 1248–1253 µs and re-arm p99 421–423 µs, the same in B1 and C1.
  - Caveat for every `*_p99` in this campaign: `LiveMetrics` keeps only the last 64 samples, so the p99 covers
    the end of a run, not the whole run.
  - Whole-run outliers are covered by the `RxSlowReady` events instead: 0 in all B1 and C1 runs.
- TX slow transmits (> run minimum + 40 µs): 0–1 per run.

Map coordination:
- In-window version mismatch: none.
- Divergence 0; schedule disagreement 0 in every run.
- The post-run `version_mismatch` flag is again the known post-pull artifact. The in-window check now
  compares only activations before the RX end frame.

Map churn with the unchanged estimator:
- C1: 21–28 exclusions and 10–16 map versions per 33 s run; minimum active 14–21.
- C(old): reaches the floor of 12 in every run.
- Fewer spurious losses in C1 also means fewer spurious exclusions. Even so, the estimator still excludes a
  channel after one isolated loss, and C1 was not run long enough to test floor thrash. That is C2's job.

## 2. Diagnostic hypotheses (not proven)
- **The interference level differs strongly between sessions.**
  - B in the Gate C session: 0.385 %. B1 in this session: 1.24 %, with CRC 3× higher.
  - Consistent with time-varying 2.4 GHz interference (Wi-Fi-consistent). C1's reduction is a same-session
    comparison and does not depend on this.
- **The remaining C1 losses look like RF loss** on still-active channels plus probe/exclusion lag. None of them
  are slow post-reads.
- **C1 being well below C(old) also lowers C1's map churn.** In C(old), cache-induced losses fed the
  estimator (positive feedback toward the floor).

## 3. Remaining unknowns
- Whether the C1 infrastructure itself changed Gate B's loss. B1 was not interleaved with the old B image in this session.
- Long-run behaviour of C1: floor thrash and convergence. This is the estimator, which C2 addresses.
- Residual core-0 flash execution not yet removed, flagged by code review:
  - IDLE0 and watchdog hooks;
  - `uxTaskGetStackHighWaterMark`, roughly every 40 ms;
  - HWCDC BUS_RESET handling.

  None produced a slow post-read event here, but they are not excluded by design.
- Post-run robustness issues found in review. They affect only boot and the post-run dumps, not the measured window:
  - an unbounded `Serial.flush()` if no host reads;
  - a control line that can be split around a pull dump.

  They are fixed in a separate follow-up commit before C2.
- Interferer identity: Wi-Fi-consistent only.

## 4. Infrastructure finding to carry forward
On this platform, at a 150 µs gap, **any code that runs rarely between frames can cost one frame per
occurrence**. This applies to control planes, logging, FEC/ARQ bookkeeping, and anything else not executed
every frame, unless that code is IRAM-resident or the RX post-read path is. Later gates (D/E) must account for it.
