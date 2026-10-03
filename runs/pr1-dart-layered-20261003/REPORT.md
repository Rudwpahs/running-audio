# PR1-DART layered board campaign — Gates A and B (issue #52), 2026-10-03

Branch `claude/pr1-dart-layered-board-20261003` (from `claude/pr1-board-run-20260929`); `main` untouched.
Boards: RX `e8:06:90:96:83:38`, TX `b8:f8:62:d9:26:b4`, ~1 m indoor, antennas vertical, placement fixed for
the whole session (`placement_20261003.jpg`). FLRC 1.3 Mbps CR 3/4, 0 dBm, 116 B. All runs: 10k packets
unless stated, 2 host progress polls. Full table: `matrix.csv` / `matrix.json`; per run: `metadata.json`,
`rx.log`, `tx.log`, `result.json`, `summary.md`.

## Verdict
- **Gate A: PASS.** Frozen images reproduce the 2026-09-29 baseline (same session as Gate B).
- **Gate B: hopping works; whether it passes depends on the threshold, so it is not declared passed.**
  TX/RX stay synchronized, and the hop overhead fits the normal RX path at 150 µs. Loss is higher than
  Gate A: pooled B is 349/72604 = 0.481 %, pooled A 9/20691 = 0.043 % (11×, z = 9.0). By the tool's
  materiality rule (Δ ≥ max(0.5 pp, 3·SE)), only 2 of 7 B runs are material (0.63 %, 0.93 %); the other
  5 are 0.25–0.42 pp above A. **Gate C was not started**; the operator decides whether this counts as a
  pass or a stop.

## 1. Measured facts
### Gate A (frozen fixed 2404 MHz images, byte-identical to 2026-09-29 prebuilds)
safe boot ok (both, `rf_enabled=0`); 5000/100 0/239; 5000/1k 1/1112; 1000/1k 0/1227; 300/10k 2/10334
(0.019 %); 150/10k 4/10346 (0.039 %) and 5/10345 (0.048 %). Scheduler misses 0, max queue 1, IRQ→RX-ready
p99 1791–1795 µs, SPI-end→re-arm-start p99 137 µs.

### Gate B (static all-40 AFH only; no channel quality / FEC / ARQ / PHY / controller)
- **Staged bring-up on the final firmware** (same session as Gate A): 5000/100 1/316; 5000/1k 1/1116
  (0.090 %, A 1/1112); 1000/1k 4/1203 (0.33 %, A 0/1227); then 150/10k as below. Earlier staged attempts
  that exposed follower bugs are kept as `*-diag1`, `*-diag2` and `*-pre`.
- **Synchronization:** boot schedule fingerprint TX == RX in every run, session 1 / map_version 1 on both
  sides, 0 schedule disagreements, no in-run resync at 5000/1000/300/150 µs after the follower fixes below.
- **Overhead:** retune (SetRfFrequency) p99 66–74 µs (max 74 RX, 129 TX), schedule compute p99 42–46 µs,
  retune failures 0. RX SPI-end→re-arm-start p99 137 → ~240 µs; IRQ→RX-ready p99 1794 → ~1900 µs.
- **Loss at 150 µs:** B runs run1–3, i2, i4, i9 and i10 measured 0.63, 0.29, 0.31, 0.43, 0.93, 0.46 and
  0.32 %. Gate A measured 0.039 and 0.048 %. Pooled, B is 11× higher (z = 9.0). The tool rule marks only
  run1 and i4 as material.
- **Loss attribution** (i9/i10, complete — 0 unattributed frames): timeouts (no IRQ) 24/13, CRC-bad 13/9,
  start-up skip 11/11.
- **Discriminating diagnostics** (150 µs unless stated):

| variant | what changed vs B | loss | reading |
|---|---|---|---|
| Bm | RX loss margin 400 → 1000 µs | 0.28 % | margin not the cause |
| B 300 µs | +150 µs idle per frame | 0.26 % (A 0.019 %) | RX idle margin not the cause |
| Bt | TX waits 200 µs after SetRfFrequency | 0.17 % | TX synthesizer settle not the cause |
| **Bs** | full hop path, every retune programs 2404 MHz | **0.029 %** | retune action / compute / follower not the cause |
| Bf29 / Bf20 | full hop path, fixed ch 29 / ch 20 | 0.145, 0.097 / 0.164, 0.077 % | single fixed channels ≈ 0.1 % |
| B (interleaved with Bf) | 40-channel hopping | 0.434, 0.925 % | hopping 3–10× worse than any fixed channel in the same window |

- **Channel distribution** (re-binned by the missed frame's channel using `analysis/afh_schedule.py`, which
  reproduces the boot fingerprint): CRC errors and no-IRQ misses cluster on the same bands (pooled χ² = 161
  for 39 dof, p < 5e-5), with no dependence on hop jump size (p = 0.29). The hot bands move between runs:
  ch 26–31 (2456–2466 MHz) in run1; ch 4–5 (2412–2414) in run2/Bm; ch 31–33 and 12 at 300 µs.
- **RX timing margin is thin at 150 µs.** A diagnostic that added two ~45 µs schedule computations to the
  RX post-read path (i7/i8) created a self-sustaining failure: every late re-arm missed the next preamble,
  so RX heard only frames that came back to the same channel, giving **97.6 % / 86.2 % loss**. Removing the
  computation from that path restored 0.32–0.46 %.

### Follower fixes made in Gate B (no new algorithm)
Each fix came from evidence. A pre-flash review led to these changes:
- the deadline is now P + min(P/8, 400 µs) after the RX-done (was 1.5 P);
- after a stall, the follower jumps over all elapsed frames at once;
- the logical frame is the nearest value that matches the 16-bit wire sequence;
- standby on the read-error path.

On-board evidence led to these:
- deadlines anchor on the frame grid of the last good packet. Before this, a truncated CRC packet
  re-anchored the grid and caused 8 cut-off frames and a resync.
- no schedule computation in the RX post-read path.

## 2. Diagnostic hypotheses (not proven)
- **Main excess loss:** band-limited 2.4 GHz interference whose position changes over time (consistent
  with Wi-Fi). Hopping exposes the link to whichever bands are busy; fixed 2404 MHz was quiet in this room.
  Consistent with: Bs ≈ A; fixed ch 20/29 ≈ 0.1 %; channel clustering of both CRC and no-sync losses.
  No spectrum or Wi-Fi scan was taken (the desktop host has no Wi-Fi adapter).
- **Fixed ch 20/29 slightly above ch 0** (0.08–0.16 % vs 0.03–0.05 %): same interference hypothesis, or a
  frequency-dependent antenna or room response. Not separated.

## 3. Artifacts and untested assumptions
- **Start-up skip, 11 frames per run.** If the frame after the first lock is lost, the follower has no
  period estimate, so it waits on that channel until the channel recurs. Seen at logical 313 → channel 22
  in every 150 µs run. This happens during acquisition, not in steady state.
- **About 1 frame lost per in-run host poll on AFH.** RX cannot retune while it prints; on a fixed channel
  it keeps receiving instead. Post-run `h`/`H` pulls block the TX loop and cause a resync after the run;
  those events are excluded.
- **RX must boot after TX.** Assumes RX acquires within ±32767 frames of the expected logical frame.
  Mid-session join or TX reboot is not handled (SyncBeacon is unused).
- p50/p95 are not measured (p99 telemetry only). Per-run variance is large over time (B 0.29–0.93 %), so
  single-run comparisons need interleaving.

## 4. Next steps (proposed, not done)
1. Confirm or reject the interference hypothesis: run a 2.4 GHz scan (for example a laptop Wi-Fi channel
   list or a spectrum app) during an interleaved B / Bf(hot) / Bf(cold) set.
2. If interference is confirmed, Gate C (adaptive channel map) is the layer that targets it. Compare C
   against these B runs, interleaved with B.
3. Before Gate C, fix the start-up skip (seed or acquire the period before relying on deadlines). This is
   a Gate B follower fix; it adds no algorithm.
4. Keep the RX post-read path free of extra work. Gate H's PROCESSING_LIMITED state should treat
   "RX re-arm late → next preamble missed" as a real overload mode on AFH.
