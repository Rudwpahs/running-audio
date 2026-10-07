# Prototype V0 — outdoor results so far (2026-10-07, park, two people)

Source: the laptop session's raw files, now in `runs/pr1-audio-prototype-20261007-field/` (summary.json + rx_pull.log per run, NOTES.md). Numbers below were checked against them.
Setup: RX (speaker) on the laptop, TX on a power bank held by the second person, fixed 2404 MHz, 150 µs gap, 94 s window, firmware `audio_rx` / `audio_tx150`
(flashed 2026-10-06, not re-flashed). Predictions are from `FIELD_TEST_CARD.md`, written before the runs. Every run: resets 0, I2S errors 0, queue drops 0, max decode 49 µs.

| run | RF loss | concealed blocks | RSSI* | prediction | verdict | heard |
|---|---|---|---|---|---|---|
| readiness check, 14 s | 0.78 % | 0 | -55 | none | – | – |
| 1 m (first) | 0.64 % | 2 (0.013 %) | -62 | < 1 % / < 0.5 % | met | not heard (operator missed it) |
| 1 m (retry) | 0.46 % | 2 (0.013 %) | -62 | < 1 % / < 0.5 % | **met** | clean |
| 5 m LOS (1st) | 4.4 % | 273 (1.78 %) | -79 | < 2 % / < 0.5 % | would have failed | not relayed |
| 5 m LOS (2nd) | 0.95 % | 9 (0.059 %) | -71 | < 2 % / < 0.5 % | **met** | clean |
| 5 m, body between | **9.97 %** | **621 (4.05 %)** | -82 | < 5 % / < 2 % | **failed both** | not reported |

\* `rssi_dbm` is the RSSI of the **last received packet only** (`pr1_live_metrics.hpp`: `last_rssi_dbm_`), one sample per run. Treat it as indicative, about ±5 dB.

## What is physically demonstrated
- Outdoors, 1 m and 5 m line of sight (2nd run): the clip was heard clean (operator report), RF loss under 1 %, 0-0.06 % blocks concealed.

## What failed or is not established
- **Body block at 5 m fails both predictions** (about 10 % loss, 4 % of audio blocks concealed; what was heard was not reported). Not tuned: the card forbids same-day tuning.
- **The 1st 5 m run (4.4 %, RSSI -79) was excluded by the operator as a setup error;** the specific error is not documented, raw data kept. The same distance gave RSSI -79 vs -71 and loss 4.4 % vs 0.95 %,
  so placement/antenna setup moves the link by about 8 dB, more than the distance step from 1 m to 5 m (-62 → -71). "5 m LOS met" rests on the 2nd run.
- **New finding: outdoor 1 m RSSI is -62 dBm, 20 dB lower than the desk (-42 dBm).** The desk boards were about 0.5-1 m apart with the antennas free; outdoors the TX was held by a person and the RX stood beside a laptop.
  Possible causes (not separated yet): hand/body detuning or shadowing the TX antenna, antenna orientation, laptop next to the RX antenna, ground reflection. This is the thin margin: the body-block run (-82) is only 20 dB below the 1 m outdoor level.
- **Loss versus RSSI in this data** (single samples): -62 dBm → 0.5-0.6 %, -71 → 0.95 %, -79 → 4.4 %, -82 → 10 %. The 5 % loss line sits near **-80 dBm**.
- **10 m, 20 m, 40 m not run.** The ladder in the card was derived from the desk level (-42 dBm at 1 m), which turned out to be 20 dB too optimistic outdoors; those predictions are superseded (see `FIELD_TEST_CARD.md`).

## Hypotheses (not measured) for the body-block failure
1. A body between two 2.4 GHz antennas costs roughly 10-25 dB; from -71 dBm at 5 m LOS that lands at the observed -82 and below the 5 % loss line. Consistent with the data, not proven.
2. TX position on the body (shoulder or head side vs belt) and the hand around the antenna.
3. Fixed channel: no frequency diversity against a static body shadow. The channel-map work (C3) reacts to interference, not to a static shadow, so it does not address this.

## Candidate fixes (none tried; each needs a prediction first, operator decides)
- **Placement first (free):** TX worn on the side facing the RX (shoulder / chest), antenna vertical, clear of the hand.
- **One discriminating experiment before any code change** (2 × 30 s silent runs at 1 m outdoors, TX on a bag with the antenna vertical and no hand on it vs the same spot with the TX held):
  prediction recorded before the run: if hand/body detuning is the main cause the resting TX reads **at least 10 dB stronger** (≥ -52 dBm); if the difference is ≤ 3 dB, the cause is geometry/ground/laptop and the hand is not the issue.
- More redundancy for audio only (already one repeat 23.5 ms later; a third copy or longer separation) costs nothing on air at the 150 µs gap.
- A lower-rate, more sensitive PHY for the audio build (audio needs about 27 kbps, the link runs 1.3 Mbps). Changes the RF baseline, so a separate decision.
- Higher TX power: needs a regulatory check first, not recommended without it.

## Status
Prototype V0 is **not complete**: audible and clean at 1 m and 5 m LOS outdoors, fails with a body between TX and RX at 5 m, long distances untested, link margin about 20 dB thinner outdoors than on the desk.
