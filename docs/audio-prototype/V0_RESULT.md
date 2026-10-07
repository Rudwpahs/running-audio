# Prototype V0 — outdoor results so far (2026-10-07, park, two people)

Source: summary written by the operator's laptop session (`C:\Users\foodl\field_kit\field_results\NOTES.md`, outside the repo, not committed).
**These numbers were relayed by that session and pasted by the operator; the raw `summary.json` / `rx_pull.log` files were not reviewed here.**
Setup: RX (speaker) on the laptop, TX on a power bank held by the second person, fixed 2404 MHz, 150 µs gap, 90 s window, firmware `audio_rx` / `audio_tx150`
(flashed 2026-10-06, not re-flashed). Predictions are from `FIELD_TEST_CARD.md`, written before the runs.

| run | RF loss | concealed blocks | prediction | verdict | heard |
|---|---|---|---|---|---|
| readiness check (silent) | 0.78 % | 0 | none (check only) | – | – |
| 1 m LOS | 0.46 % | 0.01 % | < 1 % / < 0.5 % | **met** | clean |
| 5 m LOS, 2nd run | 0.95 % | 0.06 % | < 2 % / < 0.5 % | **met** | clean |
| 5 m LOS, 1st run | 4.4 % | not relayed | < 2 % | **would have failed** | not relayed |
| 5 m, body between TX and RX | **9.97 %** | **4.05 %** | < 5 % / < 2 % | **failed (both)** | not reported |

## What is physically demonstrated
- Outdoors, 1 m and 5 m line of sight: the clip was heard clean (operator report), RF loss under 1 %.

## What failed or is not established
- **Body block at 5 m fails both predictions** (10 % loss, 4 % of blocks concealed, i.e. audible gaps are expected; what was heard was not reported).
  Not fixed or tuned: the card forbids tuning on the same day without a new prediction.
- **The first 5 m LOS run (4.4 %) was excluded by the operator as a setup error.** The reason is not documented here and the raw file was kept, not deleted.
  Taken at face value it still says something: the same distance gave 4.4 % and 0.95 % depending on the setup, so antenna orientation and placement matter more than the distance step from 1 m to 5 m.
  Until the setup error is named, "5 m LOS met" rests on the second run only.
- **10 m, 20 m, 40 m (80 m only if 40 m < 5 %) not run yet** (silent runs, predictions in the card).
- Not in the relayed note: RSSI per run, resets / I2S errors / queue drops (stop-rule fields), antenna height. They are in the laptop's `summary.json`.

## Hypotheses (not measured) for the body-block failure
1. A body between two 2.4 GHz antennas costs on the order of 10-25 dB. Free-space estimate at 5 m is about -56 dBm (1 m measured -42 dBm), so about -70…-80 dBm behind a body,
   close to the assumed -85 dBm limit of this PHY (that limit is an estimate, not verified). Check against the per-run RSSI.
2. Position of the TX on the body matters (shoulder or head side vs belt); the run held it at the body, and the antenna may have been shadowed or detuned by the hand.
3. Fixed channel: no frequency diversity against a body fade. The channel-map work (C3) does not address this; it reacts to interference, not to a static shadow.

## Candidate fixes (none tried; each needs a prediction first, operator decides)
- Placement: TX on the side facing the RX (shoulder, chest strap pointing to the listener), antenna vertical and clear of the hand. Cheapest; test first.
- More redundancy for audio only (already one repeat 23.5 ms later; a third copy or a longer separation), costs nothing on air at the 150 µs gap.
- A lower-rate, more sensitive PHY for the audio build. Audio needs about 27 kbps against 1.3 Mbps used. This changes the RF baseline, so it is a separate decision, not part of the research lane.
- Higher TX power. Needs a regulatory check first; not recommended without it.

## Status
Prototype V0 is **not complete**: audible at 1 m and 5 m outdoors, fails with the body between TX and RX at 5 m, long distances untested.
