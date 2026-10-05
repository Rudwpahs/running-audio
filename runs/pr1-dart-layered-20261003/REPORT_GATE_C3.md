# Gate C3 — strike memory + neighbour corroboration (issue #52), interim, 2026-10-04

C3 uses the C1 infrastructure and adds two mechanisms to the C2 values. The boards, placement, PHY (FLRC
1.3 Mbps CR 3/4, 0 dBm, 116 B) and gap (150 µs) are unchanged. FEC, ARQ, PHY ladder and controller are off.
Gate D is not started. `main` is untouched.

**Status: interim.** Done: offline replay, two independent code reviews, and the same-session board A/B.
**Not done yet:** a long C3 run (convergence, back-off growth) and the controlled Wi-Fi coexistence test.

## Verdict so far
- **Efficacy target met in this session.** C3n2 is **−72.0 %** vs same-session B1 (z = −33.6); C3n3 is −70.3 %.
  Both are above the ≥ 50 % target.
- **C1 is still slightly better.** The aggressive reference reached −75.6 %. C3n2 vs C1 is +14.5 % relative loss (z = 2.6).
- **N2 is the candidate to carry forward.** N3 (one bad neighbour is enough) is not better than N2 on the
  board (z = 1.2, in N2's favour) and makes more single-loss exclusions in replay.
- **No functional failure seen:** no in-window map mismatch, schedule disagreements 0, slow RX post-reads 0.
- **Not yet judged:** convergence, progressive quarantine, floor thrash over time. The 33 s runs are too
  short, and in this session every adaptive arm sat at the floor of 12.

## 1. Measured facts

### 1.1 Replay selection (commits `025c401`, `0f4485a`; `analysis/C3_REPLAY_NOTES.md`)
- **Estimator knobs, all default-off.** A golden-hash test regenerated from `afacec3` proves the defaults are bit-identical.
  - Per-channel strikes survive re-inclusion and lengthen the next probe interval (25.6 s base, 102.4 s cap).
    They are forgiven after 120 s of clean Active time.
  - Neighbour corroboration within ±2 channels.
- **Review found a hole in the first version.** A re-included channel could be excluded on a *received*
  frame, using a stale fast EWMA plus its neighbours. The reviewer reproduced it.
  - Fix: a neighbour-assisted exclusion needs a lost frame on that channel, and losses since its (re)inclusion.
  - A second reviewer ran a 36 M-step fuzz. About 117 k neighbour exclusions, every one on a lost frame with
    at least one loss since inclusion.
- **Replay results** (recent sessions / current session):

  | candidate | loss Δ recent | loss Δ current session |
  |---|---|---|
  | K0 (C1) | −63.7 % | −67.1 % |
  | K21 (C2) | −40.7 % | −46.3 % |
  | **N2** (2 bad neighbours) | −48.8 % | −53.0 % |
  | **N3** (1 bad neighbour) | −51.8 % | −56.4 % |

- **Map changes over an 1800 s looped stress:** N2 and N3 settle near 5/min; K21 stays at 11/min, K0 at
  19/min. That is bounded; the steady state is unknown.
- **Neighbour exclusions with zero losses since inclusion:** 0 in all 446 grid candidates.

### 1.2 Board: B1 → C1 → C3n2 → C3n3 ×3, 150 µs / 10k (`gateC3-interleave/`, `stats.json`)
Interference was much heavier in this session than in any earlier one: B1 lost 6.0–9.0 % with 509–810 CRC
errors per run.

| run | loss | CRC | no-IRQ fr† | ctrl ev | lost ≤ 3 fr after ctrl | slow post-read | min active |
|---|---|---|---|---|---|---|---|
| B1 i1 | 6.036 % | 509 | ≥46 | 0 | – | 0 | – |
| C1 i2 | 2.014 % | 136 | ≥54 | 112 | 3 | 0 | 12 |
| C3n2 i3 | 2.279 % | 177 | ≥40 | 63 | 2 | 0 | 12 |
| C3n3 i4 | 2.495 % | 157 | 41 | 95 | 1 | 0 | 12 |
| B1 i5 | 8.589 % | 787 | 26 | 0 | – | 0 | – |
| C1 i6 | 1.790 % | 142 | ≥32 | 116 | 3 | 0 | 12 |
| C3n2 i7 | 2.458 % | 206 | ≥22 | 81 | 0 | 0 | 12 |
| C3n3 i8 | 2.275 % | 180 | ≥27 | 80 | 0 | 0 | 12 |
| B1 i9 | 8.983 % | 810 | 25 | 0 | – | 0 | – |
| C1 i10 | 1.963 % | 147 | ≥44 | 109 | 1 | 0 | 12 |
| C3n2 i11 | 1.865 % | 139 | ≥45 | 64 | 0 | 0 | 12 |
| C3n3 i12 | 2.238 % | 165 | ≥33 | 90 | 1 | 0 | 12 |

† The 160-entry anomaly list overflowed in most runs, so no-IRQ counts marked ≥ are lower bounds. Loss and
CRC totals come from the telemetry counters and are complete.

Pooled:
- **Loss:** B1 2620/33281 = **7.872 %**; C1 **1.921 %**; C3n2 **2.201 %**; C3n3 **2.337 %**.
- **C3n2 vs B1:** −5.67 pp, 95 % CI [−6.00, −5.34], z = −33.6, **−72.0 %**. CRC −75.5 %.
- **C3n3 vs B1:** −70.3 % (z = −32.6).
- **C1 vs B1:** −75.6 % (z = −35.8).
- **C3n2 vs C1:** +0.28 pp, CI [+0.07, +0.49], z = 2.6.
- **C3n3 vs C1:** +0.42 pp, z = 3.7.
- **C3n3 vs C3n2:** +0.14 pp, z = 1.2 (not significant).

Other board facts:
- **Control traffic:** C3n2 produced fewer control events (208) than C3n3 (265) or C1 (337).
- **Coordination:** proposals = commits, divergence 0, schedule disagreement 0, no in-window version
  mismatch in the runs where it could be evaluated.
- **Timing:** RX slow post-read events 0 in all 12 runs. The C1 cache fix holds under heavy control traffic.
- **Map:** all adaptive arms reached the floor (minimum active 12) in every run, with 26–28 exclusions.

### 1.3 Replay vs board
- **Board result is above the replay prediction this time.** The replay predicted −49…−53 % for N2;
  the board gave −72 %.
- The replay corpus has B loss of 0.2–1.2 %; this session had 7.9 %. So the replay's "−13 pp optimism",
  measured on C2 in a 0.9 % session, does not carry over to a heavy-interference session.

## 2. Diagnostic hypotheses (not proven)
- **Why C1 and C3 are close here:** with this much band-limited interference, almost every bad channel
  gives repeated losses quickly, so C3's "two losses or neighbour support" rule fires almost as fast as
  C1's single-loss rule.
- **Residual ~2 % loss:** at the floor of 12 channels the remaining channels are still not clean. Either
  the interferer is wider than 28 channels, or the floor forces bad channels to stay. Per-channel analysis
  not done yet.

## 3. Remaining unknowns
- **Convergence and back-off on the board:** whether map changes decay, and whether quarantine grows for
  re-excluded channels. Needs a long C3n2 run.
- **Lighter interference:** behaviour in a session like the replay corpus (0.3–1 % B loss).
- **Recovery after the interferer stops:** the replay shows up to ~5 min. To be measured in the controlled
  Wi-Fi test.
- **Interferer identity:** Wi-Fi-consistent only.

## 4. Next (within the operator decision of 2026-10-03)
1. Long C3n2 run (≥ 5 min) for convergence and strike/back-off evidence.
2. Controlled 2.4 GHz coexistence test with a normal Wi-Fi AP or hotspot: traffic off → on → off.
3. Report on issue #52, then stop before Gate D.

## 5. Long run, 100 000 frames each, 150 µs (2026-10-06, `gateC3-long/`, predictions in `gateC3-long-PREDICTIONS.md`)
One pass B1 → C3n2 → C1 (not interleaved). Interference was light this time: B1 lost 0.89 %.

| arm | frames | lost | loss | CRC bad | sched. misses | active at end / min | map changes |
|---|---|---|---|---|---|---|---|
| B1 | 108 275 | 960 | 0.879 % | 795 | 1 | – | – |
| C3n2 | 108 798 | 238 | 0.218 % | 136 | 1 | 15 / 14 | 50 |
| C1 | 109 080 | 263 | 0.241 % | 150 | 1 | 13 / 12 | 50 |

Pooled loss: C3n2 vs B1 −0.66 pp (95 % CI [−0.72, −0.60], z = −20.9, −75 %). C1 vs B1 −72.6 %. C3n2 vs C1 −0.02 pp
(CI [−0.06, +0.02], z = −1.1), no difference. Safety: TX/RX map equal, no in-window mismatch, 0 divergence, 0 TX no-ack,
fingerprint match, scheduler misses 1 in every arm including B1 (so not caused by the adaptive code).

Predictions (written beforehand), judged:
1. Light session rule (B1 < 1 %: no claim of improvement). **Honoured:** the single non-interleaved pass cannot rule out
   drift in interference over the 17 minutes, so this is a consistent-direction observation, not a new efficacy claim.
2. Convergence (map changes lower in minutes 4–5 than 1–2): **not shown.** The activation list on the board holds only the first
   32 of 50 activations. C3n2: 32 in the first 190 s (≈10/min), last 18 in ≈134 s (≈8/min), roughly flat.
   C1: ≈17/min then ≈5/min. No per-minute series exists.
3. Back-off visible: **unobserved.** Event ring holds 160 of 884 events (first 48 s), shorter than the 25.6 s base probe times a second round.
   Final per-channel counters: C3n2 excluded 30 channels, 48 exclusions / 20 re-inclusions in total; 14 channels were excluded ≥ 2×
   (C1: 37 channels, 44 / 16, 6 channels ≥ 2×). The counts do not show C3 quarantining less often than C1; they do not show growing quarantine either.
4. Safety: held (above).

Measured facts / hypotheses / unknowns:
- **Measured:** C3n2 and C1 are equivalent in loss here; C3n2 keeps 2 more channels active at the end (15 vs 13) with no loss penalty.
- **Hypothesis:** with light, band-limited interference the C3 strike/neighbour rules fire about as fast as C1's single-loss rule, so both end up in the same place.
- **Unknown:** whether back-off lengthens probe intervals, and whether map churn decays beyond 5 min. Needs a complete exclusion/activation timeline
  (raise the activation list and event ring on the RX, cold path only, no change to the post-read path) or a longer, interleaved run.
