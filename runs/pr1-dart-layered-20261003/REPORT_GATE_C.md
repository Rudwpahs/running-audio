# Gate C — adaptive channel map over Gate B (issue #52), 2026-10-03

Branch `claude/pr1-dart-layered-board-20261003`; `main` untouched. Same boards, placement
(`placement_20261003.jpg`), PHY (FLRC 1.3 Mbps CR 3/4, 0 dBm, 116 B) and gap (150 µs) as Gates A/B.
Only `PR1_ENABLE_ADAPTIVE_MAP=1` differs between B and C images. FEC, ARQ, adaptive PHY, the controller
and jitter/Opus/PLC are all OFF.

**Control plane (bench only):** USB host relay with a two-phase commit.
1. RX sends `PR1PROP`.
2. The host forwards it as `MAP`/`PRB`; TX validates the base version and bitmap and replies `PR1ACK`.
3. The host sends `ACK`; RX commits and replies `PR1COMMIT`.
4. The host sends `COMMIT` (or `ABORT`); TX stages and replies `PR1STAGED`.
5. Both sides apply the change at the same future logical frame (lead 600, guard 100).

Probes are data frames sent on the excluded channel at one reserved frame. There is no wireless reverse
link. The pr1_channel_quality defaults are used unchanged: suspect at fast PDR < 0.9155, exclude at
< 0.85, recover at ≥ 0.9375, floor 12, probe interval 200 ms–3.2 s, reinclude after 2 of the last 3
probes succeed.

## Verdict
- **Functional: not passed.**
  - Map coordination works, the active-channel floor holds, and the probe → reinclusion path works.
  - It fails the no-thrashing criterion: once the map reaches the floor it oscillates between 12 and 13
    channels every 2–3 s.
- **Efficacy: not demonstrated (C worse than B, same session).**
  - CRC errors fall significantly under C.
  - No-IRQ losses tied to the USB control plane rise by more, so pooled loss is higher.
- Gate D was not started.

## 1. Measured facts

### Pre-C cleanups (operator items 1 and 2)
- **Two-packet acquisition.** RX stays parked until two packets give the frame period; the measured span
  starts there.
  - Before: 11 skipped frames at start-up in every 150 µs run.
  - After, B 150 µs ×2 (`preC/`): skipped frames 0, in-window polls 0, all losses attributed.
  - Loss 21/11077 (0.190 %) and 17/11039 (0.154 %).
- **No serial polling during the measured window.** One final snapshot (`poll_progress=False`).

### Interleaved B → C → B → C → B → C (150 µs, ~11.1k packets each, `gateC-interleave/`)

| run | loss | CRC-bad | no-IRQ frames | control events | C losses ≤ 3 frames after a control event | IRQ→ready p99 |
|---|---|---|---|---|---|---|
| B i1 | 0.479 % | 31 | 22 | 0 | – | 1891 |
| C i2 | 0.547 % | 6 | 55 | 75 | 45 | 1972 |
| B i3 | 0.343 % | 21 | 17 | 0 | – | 1895 |
| C i4 | 0.529 % | 14 | 45 | 69 | 39 | 1984 |
| B i5 | 0.334 % | 15 | 22 | 0 | – | 1894 |
| C i6 | 0.565 % | 7 | 56 | 79 | 43 | 1882 |

- **Pooled:**
  - B 128/33222 = 0.385 %; C 183/33436 = 0.547 %.
  - C − B = +0.162 pp, 95 % CI [+0.059, +0.265] pp, z = 3.07. **C is worse.**
- **CRC-bad:** B 67 (0.202 %) vs C 27 (0.081 %), **−60 %**, z = −4.16.
- **No-IRQ frames:** B 61 vs C 156, **+154 %**, z = 6.41.
- **Control-plane-adjacent losses:** 127/183 C losses fall within 0–3 frames after a control-plane event
  (`PR1PROP`/`PR1COMMIT` on RX, `PR1ACK`/`PR1STAGED`/`PR1ABORTED` on TX). Those windows are about 3 % of
  frames, so roughly 1 such loss would be expected by chance.
- **Diagnostic only:** C without those losses is 56/33436 = 0.167 %.
- **TX scheduler misses:**
  - The first C smoke run, with `sscanf` parsing, had 27.
  - After the lightweight parser: 1 per run.
  - Control-line handling time p99/max: TX 264 µs, RX 185–190 µs.

### Coordination and estimator behaviour (C runs + 45k long run, `gate-C/C-gap-150us-45000`)
- **TX/RX agreement:**
  - 120/120 control ids (45k run) and all ids in the interleaved runs were applied on both sides or on
    neither.
  - Schedule disagreements 0. No in-run resync.
  - Activation logs equal up to the 32-entry cap.
- The `version_mismatch` flag in the 45k result is a post-run artifact. Map v46 activated at frame 49665,
  after the final measured snapshot. During the post-run `h`/`H` pulls only TX reached that frame; RX was
  then in resync.
- **Floor:** minimum active channels seen = 12 (the floor) in every C run.
- **Over-exclusion with the default thresholds:**
  - A single loss on a channel moves it to SUSPECT (fast PDR 1.0 → 0.75).
  - On the next visit, even a successful one, fast PDR is 0.81 < 0.85, so the channel is EXCLUDED with
    reason "pdr".
  - The map fell from 40 to 12 channels within ~6400 frames (~19 s) at ~0.2–0.4 % background loss.
    28 exclusions per 10k run.
- **Probe → reinclusion (45k run):**
  - 75 probes, 74 successful. 17 reinclusions.
  - 45 exclusions in total, including re-exclusions of reincluded channels.
  - In 10k runs (33 s) no reinclusion occurs: each probe goes to a different channel, and reinclusion
    needs three probes on the same channel.
- **Thrashing:** after reaching the floor, map versions keep changing: active count 12 ↔ 13, one map
  change every ~600–900 frames, 45 versions in 49k frames.
- **Channel avoidance only partly targets the CRC bands:** C CRC-bad is 60 % lower. In the 45k run the
  CRC-heavy channels (ch 24: 26 CRC, 34: 20, 28: 17, 29: 11) were each excluded at least once. Exclusion
  was not specific to them, though: 28 of 40 channels were excluded within ~19 s, and the first ones
  (36, 1, 28, 14, 13, …) were not band-concentrated.

## 2. Diagnostic hypotheses (not proven)
- **USB control-plane I/O disturbs RX timing.** The 150 µs RX margin is tens of µs. Control-line handling
  runs only in the guarded idle window, yet losses cluster right after control events. The likely path is
  asynchronous USB-CDC interrupt and transfer work on the radio core landing on the RX post-read / re-arm
  path. Not measured at the interrupt level.
- **Default estimator thresholds are mismatched to per-visit sampling.** A channel is sampled once every
  ~40 frames; with α_fast = 1/4, a single random loss is treated as channel failure. This drives
  over-exclusion, and with it most of the control traffic and the 12 ↔ 13 thrash.
- **If both effects were removed**, the CRC reduction suggests C could fall below B. The diagnostic
  0.167 % vs 0.385 % is consistent with that, but it comes from post-hoc exclusion, not an A/B result.

## 3. Remaining unknowns
- Whether the interferer is Wi-Fi (no independent scan; "Wi-Fi-consistent" only).
- The exact mechanism of control-plane-induced loss (interrupt vs CPU vs USB stack). Whether pinning USB
  to the other core or rate-limiting control traffic removes it.
- C efficacy with calibrated thresholds (needs an operator decision; thresholds were not changed here).
- Reinclusion and thrash behaviour over longer sessions; whether the map converges.
- The event log cap (160) and activation log cap (32) truncate long runs. Per-channel counters are
  complete.

## 4. Proposed next steps (not done; need operator approval)
1. **Isolate the control plane from the RX core.** Options: move USB-CDC / serial handling off the radio
   core, or batch and rate-limit control events. Then repeat the B/C interleave to get a clean C-vs-B
   result.
2. **Calibrate the estimator for per-visit sampling** as a separate, explicitly approved change. For
   example, exclude only after ≥ 2–3 losses within a window and use the slow EWMA for exclusion. Add
   reinclusion hysteresis to stop the 12 ↔ 13 thrash.
3. **Controlled-interference A/B** (for example, one fixed 2.4 GHz source on a known band) once the
   above are fixed.
4. **Wireless reverse control link** (AFH map + ARQ feedback combined) only after C passes, as its own
   gate.
