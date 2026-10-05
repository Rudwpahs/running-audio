# Physical tests left for Prototype V0 (prepared 2026-10-06, to run next session)

Everything else is done or derived (see `DEMO_PROCEDURE.md`). Only two short physical runs remain. Sound plays in both:
**ask the operator before starting each one.** The clip is quiet and finite (2 plays, about 28 s).

## Already prepared
- Both boards are flashed with `audio_rx` (RX, COM3) and `audio_tx150` (TX, COM4). A silent desk dry run of the script worked
  (`runs/pr1-audio-prototype-20261003/dry-run-silent/summary.json`: RF loss 0.024 % over 29 s, 0 resets, 0 concealed blocks).
- Script: `runs/pr1-audio-prototype-20261003/distance_run.py` (RX stays on the PC; the TX runs on a power bank).
- Use `C:\Users\USER\.platformio\penv\Scripts\python.exe` to run it.

## Predictions (written before the runs; judge against these, do not edit afterwards)
| run | RF loss | concealed blocks | resets / I2S errors / queue drops | heard |
|---|---|---|---|---|
| 5 m LOS, 120 s window | < 2 % | < 0.5 % | 0 / 0 / 0 | no continuous crackle |
| 5 m, body between (TX on the body) | < 5 % | < 2 % | 0 / 0 / 0 | occasional tick acceptable |

Reference at 1 m: 0.90 % loss, 1 concealed block in 333 s. If a prediction fails, report it as failed; do not tune the code in the same
session without a new prediction. Stop rule: resets > 0, I2S errors, or sound stuck or muted: stop and report.

## Order (one physical action per step)
1. **Move the TX to a power bank** (USB-C, antenna vertical, away from metal and the router) at **5 m line of sight** from the RX
   (the RX stays on the PC, the speaker unchanged). The TX is silent when powered. Do not move either board during a run.
2. Operator says "시작". Ask "소리 틀어도 돼?" first. Then run:
   `python runs/pr1-audio-prototype-20261003/distance_run.py v0-5m-los --label "5 m LOS" --window 120`
   When it prints START, the operator presses the TX BOOT button once and listens. After 120 s the script writes `summary.json`.
3. Operator reports: clean / occasional ticks / continuous crackle.
4. **Body block:** same 5 m. The operator holds the TX against the body (belt or chest height, antenna vertical) with the body between
   TX and RX. Ask again, then run:
   `python runs/pr1-audio-prototype-20261003/distance_run.py v0-5m-body --label "5 m, body between" --window 120`
5. Record both in `V0_RESULT.md` (predicted vs measured, what was heard), commit, post to issue #52, then stop. Gate D stays blocked.

## Not planned (derived or not relevant)
5-minute repeat (done once at 1 m), latency measurement (derived at most about 80 ms, irrelevant for one-way music),
the TX-vanishes-from-USB investigation (no USB in the demo; recorded as an open risk).
