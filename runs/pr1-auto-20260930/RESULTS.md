# Unattended bench suite — 2026-09-30

Both boards on the desktop host at bench spacing (RSSI −40…−55 dBm). Host orchestration only
(`suite.py`). Images: frozen RF defaults + boot-only OLED role label (`claude/pr1-oled-role-label`,
merged with the build-flag overrides from #51). Progress polling is now rate-based (2 polls per 10k run).

## Headline
**Antenna placement dominated today's results.** Both whip antennas lay flat on the desk for most
of the day. In the 3 h run, lifting the TX antenna ~13° off the desk surface (operator, ~124 min)
dropped loss from ~20 % to 0.039 % at the same gap and firmware. Most "degradation" measured today
before that point is therefore a placement artefact, not a gap/firmware property.

## Step 1 — OLED image at 150 µs (10k)
10347 packets, missing 21, **0.203 %**, CRC bad 1, RSSI −47, IRQ→ready p99 1788 µs, 2 polls.
Matches 2026-09-29 150 µs (0.30 %, unpolled 0.23 %) → OLED label does not change the link.

## Step 2 — fine sweep 125–150 µs (10k each), plus controls
Environment/placement had drifted by then (RSSI −53, CRC errors ~0.5–0.7 %).

run | 150 | 145 | 140 | 135 | 130 | 125
---|---|---|---|---|---|---
forward | 3.40 % | 2.45 % | 1.32 % | 1.24 % | 1.33 % | 2.63 %
reverse (after 150 repeat 1.47 %) | 1.36 % | 1.31 % | 1.23 % | 0.96 % | 1.06 % | 4.18 %

- 130–150 µs: no gap trend; loss set by the (antenna-flat) CRC floor.
- 125 µs: missing ≫ CRC bad in both passes → the timing loss seen on 2026-09-29, reproduced.
- Image A/B/A at 150 µs (OLED-branch vs suite build): 1.61 / 1.28 / 1.12 % → firmware not the cause.

## Step 4 — CR 1/2 A/B (not baseline), antennas flat
CR 1/2 gaps 200/175/150/125: 2.24 / 4.66 / 12.6 / 2.97 %. Interleaved at 150 µs:
CR 1/2 7.33 % → CR 3/4 1.52 % → CR 1/2 24.3 % → CR 3/4 1.84 % → CR 1/2 @175 17.3 %.
Under this (flat-antenna) placement CR 1/2 was much worse and erratic, almost all CRC errors,
while interleaved CR 3/4 stayed 1.5–1.8 %. This contradicts the 20 m result (CR 1/2 better);
it was not re-tested after the antenna change, so the placement confound applies. Not causal.

## Step 3 — 150 µs long run, 3 h (one snapshot/min)
segment | span | missing | loss | CRC bad
---|---|---|---|---
0–124 min, both antennas flat | 2,587,757 | 511,295 | 19.76 % | 493,619
124–126 min, operator touch | 41,738 | 58 | 0.14 % | 56
126–180 min, TX antenna ~13° up | 1,126,931 | 445 | **0.039 %** | 145
whole run | 3,756,426 | 511,798 | 13.62 % | 493,820

- Flat-antenna segment rose 4 % → ~28 % over the first ~45 min, then wandered 14–28 %.
  RSSI stayed −53; losses were CRC errors, so not a signal-strength limit.
- After the lift: flat at 0.03–0.04 % for 54 min, RSSI −44…−46.
- Stability: no board reset (1 boot each; `micros()` wrapped twice, expected), scheduler misses 0,
  max queue depth 1, IRQ→RX-ready p99 1784–1793 µs throughout, 3 USB-output stalls recovered by
  reopening the port (no reset).
- Operator reported the boards "warm" (not hot). The slow rise while flat could be thermal drift on a
  marginal placement, but that is a hypothesis; the step change at the touch is placement.

## Implications
- Placement/antenna orientation must be controlled and recorded before any gap, distance or PHY A/B
  comparison. The 20 m +3 dBm "gain" and today's CR 1/2 regression are both suspect for this reason.
- Suggested quick check (operator, ~5 min): TX antenna flat → 13° → vertical → flat, 1 min each.
- Boards are left on the baseline OLED images (RX cr3, TX cr3 150 µs), TX antenna ~13° up.

## Antenna check (operator-moved, 60 s windows, no board reset, 150 µs, baseline OLED images)
Boards on the floor, bench spacing; only the TX antenna/board was changed, RX antenna flat throughout.

condition | span | missing | loss | CRC bad | RSSI
---|---|---|---|---|---
1. both antennas flat on floor | 20874 | 69 | 0.33 % | 53 | −51
2. TX antenna ~13° | 20874 | 39 | 0.19 % | 16 | −54
3. TX antenna vertical + TX board raised ~8 cm and rotated | 20873 | 12 | **0.058 %** | 1 | −43
4. both flat on floor again (repeat of 1) | 20873 | 172 | 0.82 % | 113 | −61

- Vertical TX gave the lowest loss (6–14× below flat) and the highest RSSI; flat placements were poorly
  repeatable (0.33 vs 0.82 %, RSSI −51 vs −61). Condition 3 also changed board height/orientation, so
  antenna angle and height are not separated. Mechanism (polarisation / near-field / surface coupling)
  not measured.
- Proposed standard for future runs: both antennas vertical, boards raised off the surface, cables away
  from the antennas, placement photographed.
