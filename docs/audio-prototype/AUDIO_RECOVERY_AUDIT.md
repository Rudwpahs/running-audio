# AUDIO_RECOVERY_AUDIT — PR1 audio prototype lane (issue #52 lane D), 2026-10-03

Branch `claude/pr1-audio-prototype-20261003`, created from `ef75c7e` (C1.1 infrastructure, no C2/C3
estimator). `main` untouched.

**Sources:**
- Audio-recovery agent: read-only search of all refs.
- Audio-hardware agent: pins.
- RF-audio integration agent: runtime hook points.

**Status codes:** 1 = verified on real board, 1* = operator note only (weak), 2 = host/simulator only,
3 = code exists, no verification record, 4 = missing.

## 1. Did the MAX98357A ever play audible music? — NOT VERIFIED (before today)
- **The only evidence is one operator note.** It is in
  `origin/checkpoint/pr1-rf-20260823-0232` @ `37b213f`, file `checkpoints/pr1-rf-2026-08-23/README.md`:
  - L8: "B receiver: … IMA-ADPCM decode -> MAX98357A speaker"
  - L68: "audible crackle is improved but not completely eliminated"
- **No objective artifact exists:**
  - No saved log contains the `[AUDIO] MAX98357A … OK` or `playback started` line.
  - There is no recording.
  - Issue #16 ("validate MAX98357A stored-audio playback without RF") is still open with every box unticked.
- **The underrun counter does not prove sound.** The August RX keeps its 5 ms playback clock even with the
  speaker disabled (`PR1_Audio_Stream.ino` L1634–1637).
- **New today: physical audio output is VERIFIED** (§5, D2-1 tone).

## 2. Component audit
"Bundle" = base64 tgz at `origin/checkpoint/pr1-rf-20260823-0232` @ `626de95`,
`checkpoints/pr1-rf-2026-08-23/bundle/*.part01-04`. Its SHA-256 `722cc774…5ccdc` matches RECONSTRUCT.md.

| Component | Status | Evidence | Reuse |
|---|---|---|---|
| 32 kHz IMA-ADPCM call sites (`imaEncodeBlock`/`imaDecodeBlock`, self-test) | 1 (RX logs imply decode) | bundle `firmware/PR1_Audio_Stream.ino` L44–49, L152–254, L1091 | partial |
| **IMA-ADPCM codec source `Pr1ImaAdpcm.h`** | **4** | included at L7 but absent from the bundle and from all git history | **must be rewritten** (standard IMA, checked against the self-test) |
| Music source | 1* | `PR1_Phone_RF_Stream.ino`: Galaxy phone over USB UAC1, 48 kHz stereo → 32 kHz mono (L30–60). Also a 1 kHz generated tone (`kGeneratedTone`, L82–92) | phone TX does not build here (needs `USB_MODE=0` + TinyUSB audio) |
| Frame format | 1 | `.ino` L28–49: 100 B payload = 14 B header + 86 B ADPCM (4 B state + 82 B nibbles), 165 samples / 5.156 ms per packet | reusable |
| TX buffering | 1* | USB ring 8192 frames, start at 960 (`Phone_RF_Stream.ino` L47–51) | partial |
| RX jitter buffer | 1 | queue 24, acquire after 6 packets, PLC fade (`.ino` L574–579, 889–900) | reusable |
| PCM ring + underrun counter | 1 | 4096-sample ring, start at 990, 160-sample chunks, `playbackUnderruns` (L575–616, 1155–1170); logs show `under=0` | reusable |
| SX1280 FLRC settings for audio | 1 | L21–26: 2476 MHz, 650 kbps, CR 1/2, 3 dBm, 225 µs gap, High-Sensitivity on, RadioLib 7.1.2 | reference only. The demo keeps the **current** frozen PHY (1.3 Mbps CR 3/4, 0 dBm) |
| I2S + MAX98357A | 1* (driver init) | `.ino` L692–697, L1205–1225: I2S_NUM_1, 16-bit, RIGHT_LEFT, 32 kHz, SD HIGH. `pin_config.h` and `Arduino_DriveBus_Library` are missing | partial |
| Raw `driver/i2s.h` setup | 3 → **1 today** | `origin/agent/pr1-h59401-prototype-v3` @ `d14845b` `src/rx_role.cpp` L147–173. Pins BCLK 40, LRCLK 41, DOUT 39, SD 38 (`include/pr1_config.h`) | **reused for D2-1** |
| Audio simulator | 2 | `tools/audio_packet_sim.py` (8 kHz u8 v0 profile) | obsolete format |
| WAV/PCM reconstruction tool | 4 | no WAV tooling on any branch | to be written |
| "155 kbps" | 1 | bundle `logs/high-sensitivity-*.txt`: `rf=155.0x kbps` | — |
| **"5 m LOS 0.055 %, body block 0.45 %, underrun 14"** | **4** | **not in any branch, commit, issue or PR**. Saved August audio logs: 33/19361 = 0.17 % (HS off), 7/17253 = 0.041 % and 71/50159 = 0.14 % (HS on), all `under=0`; no distance or body-block audio log. The only "14" is "max burst 14" in a non-audio benchmark report | treat as unverified operator memory; re-measure in D3 |
| Old wiring guide with pins 43/44/21 | 3 | `origin/agent/sx1280-flrc-poc` `docs/wiring_v2.md` | **obsolete**: GPIO21 is now the SX1280 RX enable |

**Also unusable:** `origin/checkpoint/pr1-rf-20260823-0232-exact` holds a zip truncated to 15 KB. Use the tgz bundle.

## 3. Hardware (from git; physically consistent with today's tone test)
- **Boards:** 2 × LilyGO T3-S3 **MVSR** V1.1 (`37b213f`), with an onboard MAX98357A.
- **I2S pins:** BCLK 40, LRCLK 41, DIN 39, SD_MODE 38. These are not strapping pins and do not clash with the
  SX1280: SCLK/MOSI/CS/RST = 5/6/7/8, DIO1 9, BUSY 36, TXEN 10, RXEN 21, MISO 3.
- **Avoid:** GPIO 43/44 (UART0 boot log), 0/45/46 (strapping pins), 15/47/48 (MVSR microphone).

## 4. Reusable path chosen (no redesign)
| Step | Plan |
|---|---|
| D2-1 tone | `d14845b` `configureI2s()` verbatim (plus SD held LOW until the DMA is zeroed), at the August 32 kHz rate. Task pinned to core 0, as in the August sketch |
| D2-2 stored ADPCM | Rewrite the missing `Pr1ImaAdpcm.h` as **standard** IMA-ADPCM and check it against the August self-test and assertions. Add a host WAV encoder/decoder (bit-exact) |
| D2-3 RF | Keep the **current** packet and runtime (116 B packet, 100 B payload, frozen PHY) and **fixed channel 2404 MHz**, starting at a 1000 µs gap (more RX idle window), then 150 µs. Carry self-contained ADPCM blocks: header `block_seq`, `predictor`, `step_index`, 188 samples / 94 B. RX: push the payload into an SPSC queue **after** re-arm; decode and I2S in a core-0 task. Port the August jitter/PCM-ring/underrun logic |

Known integration issue (code reading, not run on a board): at HEAD with `PR1_ENABLE_AFH=0`, the RX never
reopens the control window after the first packet. Fix this before D2-3.

## 5. Verified today
**D2-1 — test tone → I2S1 → onboard MAX98357A → speaker: PASS.**
- **Firmware:** `firmware/t3s3_audio_bringup` env `tone`. sha256 in `runs/pr1-audio-prototype-20261003/d2-1-tone/rx_serial.log`.
- **Board:** RX, COM3, MAC e8:06:90:96:83:38.
- **Signal:** 440 Hz sine, amplitude 4096, 32 000 Hz, 16-bit, RIGHT_LEFT (mono duplicated), STAND_I2S,
  DMA 8 × 160, no APLL, RF off.
- **Driver:** install / set_pin / zero all `ESP_OK`, I2S task on core 0.
- **Streaming:** 64 000 frames per 2 s (exactly 32 kHz), short_writes 0, errors 0. Mute and unmute work.
- **Audible result: the operator heard a steady tone (2026-10-03).** First physical audio-output evidence in this repo.
- **Not checked:** clean versus distorted, the 16 kHz fallback, 10-minute stability, thermal behaviour.

## 6. Not verified yet
- IMA-ADPCM codec (missing; to be rewritten).
- Music playback on the speaker.
- RF-streamed audio.
- The 5 m / body-block numbers.
- End-to-end latency.
- Effect of I2S on RX timing (the C1 cache constraint).
