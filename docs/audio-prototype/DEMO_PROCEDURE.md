# PR1 Audio Prototype V0 — demo procedure

Status: written 2026-10-06. Heard working at the desk (1 m, TX on PC USB): the RX playing, the BOOT button, the `p` command, no sound after a TX power cycle.
Not yet tried: the TX powered from a power bank, 5 m and body block (see `PHYSICAL_TESTS_TOMORROW.md`).
Branch `claude/pr1-audio-prototype-20261003`.

## What it is
Two LilyGO T3-S3 boards. The TX holds a short clip ("My Grandfather's Clock", 14 s, quiet), sends it over SX1280 FLRC
(fixed 2404 MHz, 1.3 Mbps, 0 dBm) to the RX, which plays it on its onboard amplifier/speaker. Audio = 32 kHz mono IMA-ADPCM;
every packet is repeated once 23.5 ms later; a missing block is concealed with a short fade. One-way only: no microphone, no reverse link.
The adaptive channel-map estimator (C1/C2/C3) is NOT part of the audio build.

## Before the demo (once, at the PC)
Boards: RX = COM3 (the one with the speaker connected), TX = COM4.
- Flash the RX with env `audio_rx` and the TX with env `audio_tx150` (project `firmware/t3s3_sx1280_runtime`),
  or run `python runs/pr1-audio-prototype-20261003/distance_run.py x --flash` (flashes both).
- The TX boot banner must show `PR1_AUDIO_RF ... manual_start=1`. The clip plays only on command; a reset never plays sound.

## Demo
1. Put the RX where the speaker should be (USB power from a PC, charger or power bank). Antenna vertical, off the table surface, away from metal.
2. Power the TX from a power bank (USB-C). Antenna vertical. It sends silent blocks; nothing is audible.
3. Wait about 5 s. Press the TX **BOOT button once**, short. Do not hold it, and do not press again until the sound stops.
4. The clip plays **twice (about 28 s)** and stops by itself. To play again, press BOOT again.
5. Optional from a PC: sending the character `p` to the TX serial port does the same as the button.

## If it does not work
| Symptom | Likely cause | Action |
|---|---|---|
| no sound after BOOT | RX is not running the audio firmware, or the TX was powered a moment ago | wait 5 s and press once more; check the RX was flashed with `audio_rx` |
| crackle every second or so | link degraded (distance, body, Wi-Fi nearby) | move closer, antennas vertical, TX away from metal and the router |
| a short stutter at the very start | RX jitter buffer filling (6 blocks, 35 ms) | normal |
| TX not listed on the PC USB | known open issue (the TX once dropped off USB while running) | not relevant with a power bank; replug when flashing |
| hiss with nothing playing | amplifier idle noise | remove RX power, or flash `audio_rx_noamp` |

## Known limits (do not promise more)
- Measured at 1 m: 5 min continuous, RF loss 0.90 %, 1 concealed block, no crackle heard.
- Not measured yet: 5 m line of sight, body block. Latency is derived, not measured: at most about 80 ms
  (35 ms prefill + 5.9 ms block + 40 ms I2S buffer).
- The clip is stored in the TX flash. Live microphone audio does not exist yet.
- Wi-Fi-consistent interference raises loss (fixed channel, no channel map). If it crackles, move away from the router.
