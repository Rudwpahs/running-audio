# C3n2 long run — predictions recorded before the runs (2026-10-06)

Runs, same placement, 150 us, target 100000 frames (~5.3 min each), order B1 -> C3n2 -> C1 (one pass; interference drifts, so loss is compared within this pass only):
`gateC3-long/{B1,C3n2,C1}-gap-150us-100000-L1`

Predictions (to be judged against the data, not adjusted afterwards):
1. If interference is as heavy as 2026-10-04 (B1 > 5 %): C3n2 loss <= 50 % of B1; C3n2 within +30 % relative of C1.
   If B1 < 1 % (light session): C3n2 and C1 both within the B1 band, no claim of improvement.
2. Convergence: map changes per minute in minutes 4-5 are below those in minutes 1-2 (C3n2).
   Falsified if the rate is flat or rising.
3. Back-off: at least one channel is re-excluded after probing and its next probe interval is longer
   (strike back-off visible in the exclusion log). Falsified if every re-exclusion has the base interval.
4. Safety: no in-window map mismatch, 0 slow RX post-reads, 0 scheduler misses, active channels >= 12.
Stop rules: any of the safety items above violated, watchdog/reset, unexplained sequence jump.
