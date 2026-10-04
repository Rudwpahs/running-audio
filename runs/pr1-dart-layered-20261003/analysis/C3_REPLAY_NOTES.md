# Gate C3 — offline replay notes (issue #52), revised 2026-10-04 after independent review

Replay only. No board, serial port, esptool or `gate.py run` was used; nothing was flashed or committed.
This revision fixes the defects found by the review of the first C3 replay (2026-10-03) and re-runs the
whole selection. Numbers from the first version are superseded; its stage-1/stage-2 output files were
produced with the defective rule and have been removed.

## Verdict (short)
- **Neighbour rule fixed.** A neighbour-assisted exclusion now needs a frame that was *lost on that
  channel, now, while it is in the map*. Over all 446 grid candidates and every stream set, the number of
  neighbour-assisted exclusions with zero own losses since (re)inclusion is **0**.
- **≥ 50 % target: reached in replay only marginally, and only by one neighbour variant.**
  - Selected candidate: −51.8 % on the recent sessions, −56.4 % on the current (C2-interleave) session.
  - The first C3_SEL values with the fixed rule give −48.8 % / −53.0 % (they were −49.9 % / −55.0 % with
    the defective rule), i.e. below target.
  - On the current session the replay overstated K21 by ~13 pp (replay −46.3 %, board −33.0 %). With the
    same optimism the board would give roughly −40 to −45 %. **A board result ≥ 50 % is not expected with
    confidence.**
- **Convergence: bounded, not converged.** Over 1800 s of stationary stress the map-change rate falls from
  ~9.2/min in the first 300 s to a plateau of ~4.3–5.3/min and stays there (K21: flat ~11.2/min). The
  plateau is set by the quarantine cap: every persistently bad channel cycles once per ~3 × 102 s. The
  rate does not go to zero and the true steady state on a board is unknown.
- **Price: slow recovery.** After a genuine "channel becomes clean" change, the censored mean time back
  into the map is 123 s (max 309 s) against 40 s for K21; within 100 s only 36/66 channels are back
  (K21 52/54).

## 1. Corpus (`c3_extract_streams.py` → `c3_streams/`)
Unchanged: 13 static-map Gate B/B1 runs of the C2 corpus plus the 3 current-session B1 runs
(`gateC2-interleave/B1-…-i1/i4/i7`); `gateC1-interleave/B1-i4/i7` stay excluded (truncated anomaly lists).
- `all` = 16 runs (~33 s each); `recent` = 7 runs of the sessions with a concentrated band
  (gateC, gateC1, gateC2); `c2sess` = the 3 current-session runs.

## 2. Estimator changes in this revision (`firmware/common/pr1_channel_quality.hpp`)
Every knob still defaults to off. Defaults and the C2 config are bit-identical to afacec3 (§7).

| item | before | now |
|---|---|---|
| Neighbour direct evidence | `pdr_fast < neighbor_direct_fast_q15`, evaluated on any frame; the EWMA is not reset at re-inclusion, so a **received** first frame after re-inclusion could exclude the channel (reason Neighbor, strike 2) | all of: **this frame is a loss**; the channel has ≥ N data losses **since its (re)inclusion** (`ChannelStats::losses_since_inclusion`, reset at reinstatement, counted only while Active/Suspect); `pdr_fast` below the threshold. N = number of losses that take a clean fast EWMA below the threshold (1 for 0.82, 2 for 0.60), derived in the constructor / `setConfig` |
| `neighbor_radius` | unbounded loop | clamped to 2 (IRAM loop bound: ≤ 4 neighbours) |
| `neighbor_min_bad = 0` | excluded with no bad neighbour | treated as 1 |
| `neighbor_direct_fast_q15` > 32767 | "always direct evidence" | clamped to 32767 (= one own loss on this frame); a threshold no loss run can reach switches the rule off |
| `strike_max_probe_ms` < `max_probe_ms` | silently ignored | any non-zero value replaces the cap, as the comment said |
| Strike forgiveness | ran while Active **or Suspect** | **decision:** runs only for clean Active time. Any data loss and every visit in Suspect restart the clock, so a channel held Suspect at the minimum-active floor is never forgiven |
| new knob `reinstate_reset_pdr` (default 0) | — | 1 = a re-included channel starts with clean fast/slow EWMAs. Not selected; see §6 |

`ChannelStats` gains `losses_since_inclusion` (1 byte). Not selected and not wired: `reinstate_reset_pdr`.

## 3. Replay harness changes (`c3_replay.cpp`, `c3_sweep.py`)
- **Evidence accounting fixed.** `recent[]` (last 32 data outcomes) and the loss counter are cleared at
  re-inclusion and only count frames seen while the estimator has the channel in the map. Before, they
  kept pre-quarantine losses, so "no direct evidence" could not be seen.
- **Per-reason report:** every exclusion is classified by reason and by own losses since (re)inclusion
  (`zero_loss_by_reason`, `one_loss_by_reason`).
- **Convergence:** map activations per 300 s bin (`act_per_min_by_300s`).
- **1800 s stress** (`stress` set): each of the 7 recent runs looped ×54. **Assumption: the 33 s loss
  pattern repeats unchanged for 30 min (stationary interference).** It is a stress of the algorithm, not a
  measurement; a real band moves.
- **Recovery:** in the 200 s (`long`) and 1800 s stress streams the run's top-8 Gate B channels lose every
  loss from the half-way frame on. Reported per candidate: recovered fraction, **censored mean**
  (a channel that is not back counts with the full window: 100 s / ~900 s), and the max of the recovered.
  Medians of the recovered only are survivor-biased and are no longer used in the tables.
- `c3_sweep.py` doc corrected (long = ×6 ≈ 200 s). With the C3 knobs off `c3_replay` still equals
  `c2_replay` on all shared fields (64/64 config × stream checks).
- *Single-loss exclusion* now means ≤ 1 own loss in the channel's last 32 data visits since (re)inclusion.

## 4. Search (`c3_sweep_output_grid.txt`, `c3_replay_results_grid.json`, 446 candidates)
K0, K21 and K21 × memory {off; base 12.8/25.6/51.2 s × shift 1–4, cap = base·2^shift} × decay
{0, 120 s, 300 s} × neighbour {off, N1, N2, N3, N5, N2d} × `reinstate_reset_pdr` {0, 1}.

| neighbour variant | radius | min bad | bad = | own evidence |
|---|---|---|---|---|
| N1 | 1 | 1 | excluded/probing | loss on this frame |
| N2 (first C3_SEL) | 2 | 2 | excluded/probing | loss on this frame |
| **N3 (selected)** | 2 | 1 | excluded/probing | loss on this frame |
| N5 | 2 | 2 | excluded/probing or slow EWMA < 0.97 | loss on this frame |
| N2d | 2 | 2 | excluded/probing | this frame lost and ≥ 2 losses since inclusion (threshold 0.60) |

What the grid shows:
1. **Loss on the 33 s runs depends only on the neighbour variant.** Memory, decay and reset change it by
   ≤ 0.6 pp. Recent / current session: N3 −51.8 / −56.4; N1 −49.7 / −53.7; N5 −49.7 / −54.7;
   N2 −48.8 / −53.0; N2d and off −40.7 / −46.3 (N2d never fires: the C2 fast rule acts first).
2. **The steady map-change rate depends only on the largest quarantine interval** (base·2^shift), last
   300 s of 1800 s: none ~11.3/min; 25.6 s ~10.5; 51.2 s ~8.4; 102.4 s ~5.2; 204.8 s bursts of ~4 then ~1
   (period ≈ 3 × 205 s); ≥ 409.6 s the cycle is longer than the stress and cannot be judged.
3. **Recovery cost grows with the same interval** (censored mean / max over a 900 s window, N3):
   51.2 s → 75 s / 163 s; 102.4 s → 123 s / 309 s; 204.8 s → 338 s / 568 s.
4. **`strike_decay_ms` has no effect in this data** (0, 120 s and 300 s give identical rows). A channel
   that keeps losing never earns clean time, and clean channels rarely collect strikes. The replay cannot
   tune this knob; 120 s is kept as the design value for a genuine RF change.
5. **`reinstate_reset_pdr` = 1** removes all zero-loss exclusions (they come from the stale slow EWMA of
   the C2 rule, never from the neighbour rule) at a cost of ≤ 0.6 pp.

## 5. Results (final set, `c3_sweep_output.txt`, `c3_replay_results.json`)
Δ = replayed loss vs the same Gate B frames. Totals over `all` (16 × 33 s) unless stated.
"nb-assisted (0 since incl.)" = neighbour-rule exclusions, and how many of them had zero own losses since
(re)inclusion — over `all`, `long`, `stress` and floor-30 together this is 0 for every candidate.

| candidate | Δ all | Δ recent | Δ current session | excl | re-excl | single-loss excl | nb-assisted (0 since incl.) | map changes/min | min active | detect: n, median, censored mean |
|---|---|---|---|---|---|---|---|---|---|---|
| K0_default (C1) | −47.9 % | −63.7 % | −67.1 % | 276 | 4 | 194 | 0 (0) | 21.41 | 13 | 53/53, 3.2 s, 4.9 s |
| K21_C2 | −28.2 % | −40.7 % | −46.3 % | 106 | 2 | 1 | 0 (0) | 7.68 | 25 | 45/53, 9.4 s, 14.4 s |
| C3_old_N2 (first C3_SEL values, fixed rule) | −33.0 % | −48.8 % | −53.0 % | 121 | 1 | 31 | 34 (0) | 7.80 | 23 | 46/53, 7.9 s, 12.3 s |
| **C3_SEL** (memory + N3) | **−36.5 %** | **−51.8 %** | **−56.4 %** | 144 | 1 | 83 | 90 (0) | 7.91 | 21 | 51/53, 7.8 s, 10.1 s |
| C3a memory only | −28.2 % | −40.7 % | −46.3 % | 106 | 2 | 1 | 0 (0) | 7.68 | 25 | 45/53, 9.4 s, 14.4 s |
| C3b N3 only | −36.5 % | −51.8 % | −56.4 % | 144 | 1 | 83 | 90 (0) | 7.91 | 21 | 51/53, 7.8 s, 10.1 s |
| C3c SEL + reset_pdr | −36.4 % | −51.6 % | −56.4 % | 144 | 0 | 82 | 90 (0) | 7.91 | 21 | 51/53, 7.8 s, 10.1 s |
| C3d SEL with N1 | −34.2 % | −49.7 % | −53.7 % | 129 | 1 | 57 | 63 (0) | 8.03 | 25 | 51/53, 8.5 s, 10.9 s |
| C3e SEL with N5 | −33.9 % | −49.7 % | −54.7 % | 135 | 1 | 43 | 46 (0) | 8.73 | 22 | 50/53, 6.8 s, 10.4 s |
| C3h SEL, shift 3 (cap 204.8 s) | −36.5 % | −51.8 % | −56.4 % | 144 | 1 | 83 | 90 (0) | 7.91 | 21 | 51/53, 7.8 s, 10.1 s |
| C3j SEL, base 12.8 s (cap 51.2 s) | −36.5 % | −51.8 % | −56.4 % | 144 | 1 | 83 | 90 (0) | 7.91 | 21 | 51/53, 7.8 s, 10.1 s |
| C3l SEL + window 6/6 | −36.8 % | −52.0 % | −56.4 % | 143 | 0 | 83 | 91 (0) | 7.21 | 21 | 51/53, 7.8 s, 10.1 s |

1800 s stationary stress (7 recent runs × 54 loops) and recovery:

| candidate | map changes/min per 300 s (0–300 … 1500–1800 s) | re-excl | max strikes | zero-loss excl (all from the C2 slow rule) | min active | recover in 100 s: fraction, censored mean | recover in 900 s: fraction, censored mean, max |
|---|---|---|---|---|---|---|---|
| K0_default | 19.3 / 19.2 / 19.5 / 19.0 / 19.2 / 19.5 | 2083 | 36 | 6 | 19 | 62/66 (94 %), 46 s | 46/46, 49 s, 95 s |
| K21_C2 | 11.1 / 11.3 / 11.3 / 11.2 / 11.3 / 11.2 | 1492 | 36 | 40 | 19 | 52/54 (96 %), 40 s | 46/46, 40 s, 108 s |
| C3_old_N2 | 9.1 / 4.6 / 5.1 / 4.8 / 5.4 / 5.1 | 636 | 8 | 12 | 19 | 33/61 (54 %), 62 s | 48/48, 157 s, 316 s |
| **C3_SEL** | **9.2 / 4.3 / 4.5 / 5.0 / 5.2 / 5.3** | 655 | 8 | 15 | 19 | 36/66 (55 %), 59 s | 53/53, 123 s, 309 s |
| C3b N3 only | 10.9 / 11.3 / 11.4 / 11.1 / 11.4 / 11.3 | 1494 | 25 | 38 | 19 | 57/64 (89 %), 43 s | 49/49, 48 s, 131 s |
| C3c SEL + reset_pdr | 9.1 / 4.4 / 4.5 / 5.1 / 5.3 / 5.3 | 658 | 8 | 0 | 19 | 37/65 (57 %), 57 s | 55/55, 113 s, 295 s |
| C3h SEL, shift 3 | 9.2 / 4.3 / 3.5 / 1.2 / 4.2 / 0.6 | 446 | 6 | 11 | 19 | 36/66 (55 %), 59 s | 56/56, 338 s, 568 s |
| C3j SEL, base 12.8 s | 9.8 / 8.5 / 8.3 / 8.4 / 8.7 / 8.6 | 1064 | 12 | 27 | 19 | 58/66 (88 %), 47 s | 49/49, 75 s, 163 s |
| C3l SEL + window 6/6 | 6.8 / 5.0 / 0.7 / 4.0 / 1.5 / 3.6 | 397 | 5 | 0 | 19 | 52/68 (76 %), 61 s | 56/56, 261 s, 608 s |

Floor-30 stress (`minimum_active_channels` = 30):

| candidate | Δ all | reversals (all) | single-loss | time at floor (all) | 1800 s: reversals/min | 1800 s: map changes/min, last 300 s | 1800 s: time at floor |
|---|---|---|---|---|---|---|---|
| K0_default | −38.6 % | 46 | 145 | 50 % | 14.85 | 19.1 | 59 % |
| K21_C2 | −25.5 % | 10 | 1 | 9 % | 5.09 | 11.2 | 41 % |
| C3_old_N2 | −30.5 % | 12 | 21 | 14 % | 2.87 | 3.5 | 80 % |
| **C3_SEL** | −32.4 % | 10 | 58 | 21 % | 2.76 | 3.3 | 87 % |
| C3c SEL + reset_pdr | −32.2 % | 10 | 57 | 21 % | 2.73 | 3.2 | 86 % |
| C3h SEL, shift 3 | −32.4 % | 10 | 58 | 21 % | 2.07 | 1.3 | 89 % |

Exclusions of C3_SEL on `all` by reason: fast 40, slow 14, neighbour 90. Of the 90 neighbour-assisted
ones, 0 had no own loss since (re)inclusion and 62 had exactly one.

## 6. Selected candidate: **C3_SEL** = K21 + exclusion memory + ±2 neighbour corroboration, 1 bad neighbour
Only `neighbor_min_bad` differs from the first C3_SEL (2 → 1).

| field | value |
|---|---|
| `exclude_pdr_q15` / `exclude_slow_pdr_q15` | 22937 (0.70) / 31129 (0.95), as C2 |
| `reinstate_probe_successes` / `reinstate_probe_window` | 3 / 3, as C2 |
| `initial_probe_ms` / `max_probe_ms` | 3200 / 25600, as C2 |
| `strike_backoff_max_shift` | 2 |
| `strike_probe_ms` | 25600 |
| `strike_max_probe_ms` | 102400 |
| `strike_decay_ms` | 120000 |
| `neighbor_radius` | 2 |
| `neighbor_min_bad` | **1** (was 2) |
| `neighbor_bad_slow_q15` | 0 (only excluded/probing neighbours count) |
| `neighbor_direct_fast_q15` | 26869 (0.82 → one own loss, on the current frame) |
| `probation_visits` / `reinstate_reset_pdr` | 0 / 0 |
| runtime `PR1_MAP_MIN_INTERVAL_MS` | 5000, as C2 |

**`gate.py` C3_FLAGS:** change `-D PR1_Q_NEIGHBOR_MIN_BAD=2` to `-D PR1_Q_NEIGHBOR_MIN_BAD=1`. The other
seven C3 flags stay as they are. (`gate.py` was not edited by this lane.)

Why:
- It is the only neighbour variant above 50 % on the recent sessions in replay (−51.8 %); N2, the first
  selection, is at −48.8 % once the rule is fixed.
- Its map-change rate, floor-30 reversals and minimum active count are no worse than N2's.
- Memory 25.6 s / shift 2 / cap 102.4 s is kept: it halves the steady map rate against K21 with a worst
  recovery of ~5 min. Shift 3 lowers the rate further in bursts but doubles the recovery (max ~9.5 min);
  base 12.8 s recovers in ~75 s but only reaches ~8.5/min. This is a trade-off, not a dominated choice.
- Conservative fallback if the single-loss exclusions prove harmful on the board: N2
  (`PR1_Q_NEIGHBOR_MIN_BAD=2`, the current flags) at −48.8 % / −53.0 %.
- `reinstate_reset_pdr` = 1 is the cleaner design (no exclusion of any kind with zero losses since
  inclusion, 15 → 0 in the stress) at −0.2 pp. It is not selected only because it needs a new build
  macro (e.g. `PR1_Q_REINSTATE_RESET_PDR`) in `pr1_adaptive_map_runtime.hpp`, which this lane may not edit.

## 7. Tests
- `tests/test_channel_quality_c3.cpp`:
  - `defaultsUnchanged`: golden state-trajectory hashes over 200 000 steps for the default and the C2
    config. The trace now also feeds data outcomes to Excluded/Probe channels (as the firmware does during
    the 600-frame lead) and hashes all 40 channels. Goldens regenerated from
    `git show afacec3:firmware/common/pr1_channel_quality.hpp` (`-DPR1_C3_GOLDEN_HEADER=…`):
    `0x25c3cf156c160ece` / `0xc299ce2126e24e3c`; the current header reproduces both.
  - Regression for the review repro (9 and 11 excluded, 10 excluded by 4 losses, 3/3 probes, first frame
    received): not excluded, strikes stay 1, no Neighbor exclusion on any received frame; also with 15
    losses fed while excluded.
  - Edge channels 0 and 39; `neighbor_bad_slow_q15`; Probe-state neighbours; radius clamp;
    `neighbor_min_bad = 0`; direct threshold > 1.0 and unreachable; threshold 0.60 with a stale EWMA;
    probe-window clamp (0, 200, more successes than the window); strike cap below `max_probe_ms`;
    no forgiveness while Suspect at the floor; `reinstate_reset_pdr`.
- All 20 host C++ tests in `tests/` pass with `-std=c++17 -Wall -Wextra -Werror -pedantic`
  (`test_fixed_link_runtime.cpp` with `-Wno-type-limits`); the 34 Python tests pass.

## 8. Known weaknesses
1. **The target is not robustly met.** −51.8 % is 1.8 pp over the line on 7 runs, picked as the best of
   five neighbour variants, in a replay that was ~13 pp optimistic for K21 on the current session.
2. **83 single-loss exclusions** (K21: 1, N2: 31, K0: 194). One own loss plus one excluded channel within
   ±2 is enough. This is the C1 failure mode in a weaker form.
   - On the 9 older, cleaner runs N3 makes 24 neighbour-assisted exclusions (N2: 3) for −8.3 % vs −3.8 %,
     and two of those runs end at 27 and 30 active channels (N2: 35 and 37). The band can creep outward
     from any excluded channel as its neighbours take their occasional loss.
   - Under floor 30 it spends 21 % of the time at the floor (N2 14 %, K21 9 %); 87 % in the 1800 s stress.
3. **Zero-loss exclusions still exist, from the C2 slow-EWMA rule** (15 in the 1800 s stress; K21 has 40):
   a re-included channel's slow EWMA is still below 0.95 and it is excluded again on its second received
   frame. None are neighbour-assisted. `reinstate_reset_pdr` = 1 removes them but is not wired.
4. **Convergence is bounded, not shown.** ~5 map changes/min persist for 30 min; probes cannot tell a
   3–5 % channel from a clean one, so every quarantined channel returns and is excluded again. Strikes grow
   to 8 with no further effect (the interval is capped at shift 2).
5. **Slow recovery after a genuine RF change**: censored mean 123 s, max 309 s; only 55 % back within
   100 s. Plan ≥ 6 min for a "traffic off" phase.
6. **`strike_decay_ms` is untested by data** (§4.4).
7. **Model limits.** Looped streams assume stationary interference; the nearest-visit outcome model reuses
   observations when the map is small; detection is measured from run start on 33 s runs only; the looped
   Δ (−93 %) is not a prediction.
8. **Firmware diagnostics:** the runtime's `Excluded` event value still reports only "losses" or "pdr";
   a neighbour-assisted exclusion is logged as "pdr" (`main.cpp` / runtime are off-limits for this lane).

## 9. Files
- `firmware/common/pr1_channel_quality.hpp`: fixed neighbour rule, clamps, strike cap and forgiveness
  semantics, `losses_since_inclusion`, default-off `reinstate_reset_pdr`.
- `tests/test_channel_quality_c3.cpp`: see §7.
- `analysis/c3_replay.cpp`: evidence accounting, per-reason zero/one-loss counts, 300 s bins, `reset_pdr`
  and `bin_ms` parameters. `C3_TRACE=1` prints the event trace to stderr.
- `analysis/c3_sweep.py`: default = final set → `c3_replay_results.json`, `c3_sweep_output.txt` (~1 min);
  `--grid` = the 446-candidate review grid → `c3_replay_results_grid.json`, `c3_sweep_output_grid.txt`
  (~18 min). Needs `C:\Ruby33-x64\msys64\ucrt64\bin\g++.exe`.
- Removed: `c3_replay_results_stage1/2.json`, `c3_sweep_output_stage1/2.txt` (defective rule).
