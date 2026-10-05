# Gate C3 close-out (issue #52), 2026-10-06

Scope: the operator decision of 2026-10-03 (C3 = C2 safety + exclusion memory/back-off + neighbour corroboration + probation window).
This note closes Lane R without further board runs: what the board and the replay measured, what is derived from the code, what was not measured.
Gate D, wireless reverse link, adaptive PHY, cross-layer controller and the merge to `main` stay blocked. `main` is untouched.

## 1. Candidate to carry forward: C3n2
Build flags (`gate.py::_c3_flags(2)`, boot line `c3_cfg=` / `quality_cfg=`): C2 values (fast shift 2, slow shift 5, exclude 4 losses or fast < 22937,
slow-exclude < 31129, min active 12, probe interval 3.2–25.6 s, 3 of 3 successful probes to re-include, map change gap 5 s) plus
strike back-off shift 2 (base 25.6 s, cap 102.4 s, one strike forgiven per 120 s of clean inclusion) and neighbour corroboration
(radius ±2, ≥ 2 bad neighbours, own direct evidence fast < 26869 and a loss since re-inclusion). All C3 knobs default off; defaults are bit-identical to `afacec3` (golden-hash test).

## 2. Measured (board, same placement / PHY / power / 150 µs)
| session | B1 | C1 | C3n2 | note |
|---|---|---|---|---|
| 2026-10-04 heavy interference, 3×33 s interleaved | 7.87 % | 1.92 % | 2.20 % (−72 %, z −33.6) | C3n3 not better than N2 |
| 2026-10-06 light, 1×~5.3 min | 0.879 % | 0.241 % | 0.218 % (−75 %, z −20.9) | C3n2 vs C1 z −1.1 |
| 2026-10-06 hotspot off / on / off, 30k each | 0.58 / 2.80 / 0.50 % | – | 0.24 / 0.43 / 0.24 % | on: −84.6 % vs B1, z −24.5 |

Safety in every run: TX/RX divergence 0, no in-window version mismatch, 0 slow RX post-reads, scheduler misses 1 in every arm (also B1), active ≥ 12.
One raw final-state version mismatch (hotspot-on run) was reviewed independently and is snapshot skew (see `REPORT_GATE_C3.md` §6).
Interferer: Wi-Fi-consistent only (wide contiguous band, traffic dependent).

## 3. Derived from the code, not measured: recovery after an interferer stops
Mechanism (`pr1_channel_quality.hpp`: `probeIntervalMs`, `observeProbe`): an excluded channel is probed; it returns after 3 successful probes out of the last 3.
A failed probe doubles the next interval (3.2 → 25.6 s). A successful probe resets that doubling, but not the strike term.
- 1 strike: interval back to 3.2 s after a success → at most one wait of ≤ 25.6 s plus two probes of 3.2 s ≈ **≤ 32 s**.
- 2 strikes: interval 25.6 × 2 = 51.2 s → ≤ 3 × 51.2 ≈ **≤ 154 s**.
- ≥ 3 strikes: interval 25.6 × 4 = 102.4 s (the cap) → ≤ 3 × 102.4 ≈ **≤ 307 s (~5.1 min)**.
Strikes are forgiven only while a channel is included and clean (1 per 120 s), so an excluded channel earns no forgiveness. The worst case matches the
~5 min recovery seen in the offline replay (`analysis/C3_REPLAY_NOTES.md`). The cost of the back-off is slower re-use of channels after the interferer leaves;
the map stays above the 12-channel floor, so it does not by itself raise loss. This is a bound from reading the code and the replay, not a board measurement.

## 4. Convergence and back-off
Covered by host tests (golden hashes, 36 M-step fuzz with no neighbour exclusion without a fresh loss on that channel) and the 1800 s looped replay
(map changes settle near 5/min vs 11/min for C2, 19/min for C1). The board runs (§5/§6 of `REPORT_GATE_C3.md`) did not show decay of the map-change rate
within 5 min because the RX keeps only the first 32 activations and 160 events; that is a logging limit, not evidence against convergence.

## 5. Not measured, and why it is acceptable to stop here
- Recovery time inside one continuous run on the board: bounded by §3, no extra run planned.
- Back-off lengthening in an event log on the board: shown in the replay and unit tests only.
- Lighter-than-today interference with C3n3, other channels/positions, other PHY: out of scope of this gate.
- Reviewer caveats on the harness: the in-window agreement compares activation logicals, not channel bits per version; `cut_logical` and the quality events come from different pulls.

## 6. Recommendation (the operator decides)
Accept C3n2 as the C3 result; take no further C3 board runs. Gate D / FEC stays blocked until the operator opens it.
