# Gate C2 — estimator calibration (issue #52), 2026-10-03

C2 builds on the C1 infrastructure at `ccbe136`, plus the post-review fixes in `ef75c7e`, which touch only
boot and post-run. The calibration was chosen by **offline replay first**, then tested on the board.
- **Unchanged from Gates A–C1:** the boards, placement, PHY (FLRC 1.3 Mbps CR 3/4, 0 dBm, 116 B) and gap (150 µs).
- **Still OFF:** FEC, ARQ, adaptive PHY, controller, Opus/jitter/PLC.
- **Not started:** Gate D, the wireless reverse link.
- `main` untouched.

## Verdict
- **Functional: partial (NOT PASS on convergence).**
  - **Met:** no map-version mismatch and no sync disagreement. No floor thrash; the floor is never reached
    (minimum 22 in 147 s). Probe/re-inclusion works. No RX timing regression. Map update rate is bounded
    (≤ 1 per 5 s).
  - **Not met:** the map does **not converge**. In the 147 s long run, activations continue at the 5 s cap
    for the whole run.
  - The cause is a slow cycle of the Wi-Fi-consistent band channels (23–32): excluded → re-included after
    3/3 probes → excluded again, about every 70 s per channel.
- **Efficacy: statistically lower than B, practical target NOT met.**
  - C2 vs same-session B1: **−33 %** (z = −4.4).
  - Interleaved C2 is 0.60 %, against a target of ≥ 50 % reduction or ≤ ~0.2 %.
  - **C2 is worse than C1 with the uncalibrated estimator** (C1 −66 %, C2 vs C1 z = +5.7).
- **Calibrating the conditions the operator required costs efficacy on this data.**
  - The conditions: no single-loss exclusion, re-inclusion more conservative, rate cap.
  - The offline replay predicted this before the board test, and the board reproduced the prediction
    (replay −33.5 %, board −33 %).

## 1. Measured facts

### 1.1 Offline replay harness (`analysis/c2_extract_streams.py`, `c2_replay.cpp`, `c2_sweep.py`)
- **Input: 13 static-map Gate B runs at 150 µs**, `analysis/c2_streams/`.
  - All 40 channels are observed about every 40 frames.
  - Every loss is attributed (CRC / no-IRQ / jump).
  - Two B1 runs are excluded because their anomaly list overflowed.
  - Four older runs (`gate-B` run1–3, i2) miss their pre-fix 11-frame start-up skip; this is flagged, not corrected.
- **Replay engine:** the firmware `Estimator` and `Scheduler` themselves, compiled on the host. The runtime
  proposal rules are re-implemented: one proposal in flight, map before probe, commit after 4 frames,
  activation at +600, probe frame at +300.
- **Outcome model:**
  - A replayed frame on channel c takes the outcome of the Gate B frame nearest in time on channel c.
  - Approximation: per-channel, time-varying loss and cross-channel burst timing are kept; a single loss
    can be reused when fewer channels are active.
- **Validation against the boards:**
  - Default config on the recent sessions: replay −59 % vs board C1 −66 to −72 %.
  - Selected K21: replay −33.5 % vs board C2 −33 %.
  - Exclusion counts and map-change rates match the board within the run-to-run spread.
- **The contradiction the operator pointed out, quantified:** with the defaults,
  **156 of 218 exclusions (72 %) followed a single loss** in the channel's last 32 data visits.
- **Hindsight oracle:** excluding the k channels with the most loss per run, known only after the fact.
  - k = 8 → −64 %; k = 12 → −81 %.
  - This is an upper bound; no online estimator can see it.

Candidates (all runs / the 4 recent runs with a concentrated band). "single-loss excl" = exclusions with
≤ 1 loss in the channel's last 32 data visits:

| candidate | loss Δ all | loss Δ recent | single-loss excl | map act/min | up/down reversals | excl | re-incl | min active |
|---|---|---|---|---|---|---|---|---|
| K0_default (Gate C/C1) | −36.9 % | −59.2 % | **156** | 21.2 | 14 | 218 | 14 | 13 |
| K1 fast < 0.70 | −10.7 % | −23.2 % | 0 | 6.6 | 10 | 46 | 9 | 27 |
| K3 K1 + 3/3 probes, 1.6 s | −11.9 % | −26.2 % | 0 | 6.0 | 8 | 41 | 6 | 29 |
| K5 K1 + slow < 0.95 | −15.0 % | −27.5 % | 1 | 13.1 | 32 | 78 | 26 | 27 |
| K9 K5 + 3/3, 1.6 s, cap 3 s | −16.5 % | −31.8 % | 0 | 9.8 | 23 | 71 | 19 | 29 |
| K12 K1 + slow < 0.965 | −23.0 % | −41.6 % | 19 | 16.0 | 36 | 103 | 29 | 29 |
| K16 slow < 0.965, 3/3, 3.2 s, cap 3 s | −25.7 % | −46.8 % | 18 | 11.8 | 12 | 101 | 15 | 26 |
| K20 K5 + 3/3, 1.6 s, cap 5 s | −15.7 % | −30.5 % | 0 | 7.5 | 14 | 69 | 15 | 28 |
| **K21 K5 + 3/3, 3.2–25.6 s, cap 5 s** | **−17.9 %** | **−33.5 %** | **0** | **6.9** | **8** | 67 | 11 | 27 |
| floor-30 stress, K0 | −26.9 % | −48.9 % | 119 | 17.4 | **38** | 153 | 27 | 30 |
| floor-30 stress, K21 | −14.8 % | −26.6 % | 0 | 6.8 | **8** | 65 | 12 | 30 |

All 22 candidates: `analysis/c2_sweep_output.txt` and `analysis/c2_replay_results.json`.

**Selection.** K21 is the smallest set that meets every stated condition:
- no single-loss exclusion;
- persistent evidence, from the slow EWMA as an auxiliary rule;
- re-inclusion more conservative than exclusion;
- the floor kept;
- update-rate cap;
- the fewest reversals, including under the floor-30 stress test.

Candidates with more efficacy break the single-loss rule: K12/K16 have 18–19 single-loss exclusions, K0 has 156.

### 1.2 Code change (smallest that expresses K21)
- **Two `quality::Config` knobs in `pr1_channel_quality.hpp`.** The defaults reproduce Gate C exactly,
  checked by `tests/test_channel_quality_c2.cpp::defaultsUnchanged`.
  - `exclude_slow_pdr_q15`: a SUSPECT channel is also excluded when its slow EWMA is below this value.
  - `reinstate_probe_successes`: successes needed out of the last 3 probes.
- **Runtime:** map-update cap `PR1_MAP_MIN_INTERVAL_MS`. The estimator config is set from build macros
  (`amap::qualityConfig()`).
- **C2 images:** `exclude_pdr` 0.70 (22937), `exclude_slow_pdr` 0.95 (31129), re-include 3/3,
  probe 3200 → 25600 ms, cap 5000 ms. All other values are the defaults.
  - Boot line: `quality_cfg=...,exclude_q15:22937,...,probe_ms:3200-25600,...,exclude_slow_q15:31129,reinstate_successes:3,map_min_interval_ms:5000`.
- **Unit tests** (`tests/test_channel_quality_c2.cpp`):
  - a single isolated loss never excludes (50 repetitions);
  - 2 losses within 3 visits exclude;
  - 1-in-6 persistent loss excludes through the slow EWMA;
  - 2 of 3 probes do not re-include, 3 of 3 do;
  - the probe interval is respected.

### 1.3 Board: same-session interleave B1 → C1 → C2 ×3, 150 µs / 10k (`gateC2-interleave/`, `stats.json`)
| run | loss | CRC | no-IRQ fr | ctrl ev | lost ≤ 3 fr after ctrl | slow post-read | map v / min active | excl / probes |
|---|---|---|---|---|---|---|---|---|
| B1 i1 | 0.797 % | 26 | 62 | 0 | – | 0 | – | – |
| C1 i2 | 0.287 % | 10 | 22 | 79 | 1 | 0 | 14 / 21 | 22 / 12 |
| C2 i3 | 0.549 % | 20 | 41 | 52 | 1 | 0 | 7 / 30 | 10 / 10 |
| B1 i4 | 0.933 % | 21 | 82 | 0 | – | 0 | – | – |
| C1 i5 | 0.287 % | 13 | 19 | 81 | 1 | 0 | 14 / 18 | 22 / 13 |
| C2 i6 | 0.666 % | 23 | 51 | 55 | 0 | 0 | 6 / 29 | 11 / 11 |
| B1 i7 | 0.969 % | 38 | 69 | 0 | – | 0 | – | – |
| C1 i8 | 0.341 % | 14 | 24 | 81 | 3 | 0 | 15 / 18 | 23 / 12 |
| C2 i9 | 0.593 % | 25 | 41 | 57 | 3 | 0 | 6 / 30 | 14 / 13 |

Per-run no-IRQ counts and all other fields are in `stats.json`.

Pooled:
- **B1** 298/33134 = **0.899 %**; **C1** 102/33409 = **0.305 %**; **C2** 201/33370 = **0.602 %**.
- **C2 vs B1:** −0.297 pp, 95 % CI [−0.428, −0.166], z = −4.44, **−33.0 %**. CRC −20.6 % (z = −1.4); no-IRQ −38 % (z = −4.4).
- **C1 vs B1:** −66.1 % (z = −9.9).
- **C2 vs C1:** +97 % (z = +5.7). C2 is significantly worse than C1.

Functional details (interleave):
- In-window version mismatch: 0. Divergence: 0. Schedule disagreement: 0.
- Proposals = commits in every run.
- RX control handling: p99 ≤ 7 µs. RX slow post-read events: 0. IRQ→ready p99 1885–1899 µs in every run.
- Map: 6–7 versions per 33 s run (cap), floor never reached (minimum 29–30).
- No re-inclusion within 33 s: probes at ≥ 3.2 s intervals need 3/3 successes.

### 1.4 Long run, C2, 49 150 packets / 147 s (`gate-C2/C2-gap-150us-45000`)
- **Loss:** 176/49150 = 0.358 %. CRC 80.
- **Map:** 24 activations in 147 s. The spacing is 1667–1678 frames (the 5 s cap) almost throughout, with two
  longer quiet gaps (6018 and 6319 frames). That is **no convergence**.
  - 29 exclusions and 13 re-inclusions, from the complete per-channel counters (the event list overflowed:
    551 > 160).
  - Probes: 64 ok / 3 failed.
  - Minimum active 22, final active 23.
- **The cycle:** band channels 20/23/24/27/28/30/31/32/34 each went excluded → re-included → excluded
  again (2 exclusions, 1 re-inclusion) over the run.
- **Coordination:** 91/91 control ids two-sided, divergence 0, in-window mismatch none.

## 2. Diagnostic hypotheses (not proven)
- **Detection speed drives efficacy here.**
  - The interferer is band-limited and intermittent: channels 23–32 lose 1–5 % (Wi-Fi-consistent).
  - Requiring two or more losses delays exclusion by tens of visits per channel. The uncalibrated estimator
    excludes on the first loss, so it avoids the band sooner and over-excludes cheaply (40 channels is plenty).
  - That is why C1 beats C2 even though C1 mostly excludes randomly.
- **Probe-based re-inclusion cannot tell a 3–5 % channel from a clean one.** 3/3 probes pass with
  ~90 % probability. So bad-band channels are re-included and re-excluded in a slow cycle, and the map
  does not settle.
- **Candidates that would address both** (not implemented, not replayed yet):
  - per-channel exclusion memory: a probe back-off that grows with repeated exclusions;
  - band or neighbour corroboration: one loss next to excluded or lossy neighbours counts as persistent evidence.

## 3. Remaining unknowns
- Whether the replay's nearest-visit outcome model holds outside these sessions. It matched the board in two
  configurations here, but it reuses observations when the map is small.
- Behaviour at the floor of 12 on the board. The C2 board runs never reached it; the floor-30 replay stress
  test is the only evidence.
- Long-run convergence with the candidates in §2.
- Interferer identity: Wi-Fi-consistent, not identified.

## 4. Proposed next steps (need operator decision; none started)
1. **Choose the trade-off explicitly:**
   - (a) accept the C1 estimator's efficacy (−66 %), with its single-loss exclusions and the floor thrash
     seen in Gate C; or
   - (b) keep the K21 constraints and extend the estimator with exclusion memory and band corroboration.
     Replay these first, as C2 was.
2. **Controlled-interference A/B** (known bad band on, then off), to measure exclusion, re-inclusion and
   convergence timing directly.
3. Gate D and the wireless reverse link stay blocked until C passes.
