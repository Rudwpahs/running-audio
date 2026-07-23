# PR1 Architecture v2 — SX1280 PoC

Status: **CURRENT TECHNICAL SOURCE OF TRUTH**

Date: 2026-07-23

This document supersedes the old Wi-Fi/UDP technical specification in
`AI_HANDOFF.md`. The old specification is retained only as project history.

## 1. What PR1 is validating now

PR1 is testing whether a small, screen-light audio system can move audio from a
source device to a separate wearable/listening receiver without relying on
Bluetooth or Wi-Fi for the PR1 RF link.

The immediate goal is not a sellable product. It is to answer the highest-risk
technical questions with the hardware already selected for the first PoC.

### First PoC hardware

- 2 x LILYGO T3-S3 MVSR
- Exact radio option: **SX1280 2.4G Without PA + MVSR [H594-01]**
- ESP32-S3 based controller board
- MVSR audio board with onboard MAX98357A speaker amplifier
- 1 x Adafruit Bone Conductor Transducer #1674, 8 ohm / 1 W
- microSD used as the first controlled audio source

The LILYGO board is a development platform. It is **not assumed to be the final
production PCB**. If the PoC works, the production path is expected to move to
a certification-friendly custom PCB or a suitable certified radio module.

## 2. Architecture reset: why Wi-Fi/UDP v1 is obsolete

The repository previously locked an ESP32-S3 Wi-Fi SoftAP/UDP architecture with
a 16-byte header and a 640-byte PCM payload. That was useful as a PC/network
learning model, but it no longer represents the selected PR1 radio path.

Current PR1 transport:

```text
controlled audio source (microSD first)
        -> PCM frame
        -> optional codec
        -> PR1 RF frame
        -> fragmentation if needed
        -> SX1280 packet mode
        -> reassembly
        -> optional decode
        -> MAX98357A
        -> transducer / speaker
```

Wi-Fi may still be used on a PC as a simulator or development convenience, but
it is not the target PR1 wireless link.

## 3. SX1280 packet constraints that drive the design

Semtech's SX1280 supports LoRa, FLRC and (G)FSK/MSK packet modes. The important
application-level constraint is that a single RF packet cannot carry the old
640-byte audio frame.

Design limits used by the PC tools in this repository:

| Mode | Maximum RF payload used for budgeting | PR1 v2 header | Application bytes left |
|---|---:|---:|---:|
| FLRC | 127 B | 10 B | 117 B |
| GFSK | 255 B | 10 B | 245 B |

These values are packet-format limits, **not evidence that a given mode,
bitrate, power, antenna, or outdoor operating configuration is lawful or
reliable in Korea**. Regulatory applicability must be checked separately before
open-air RF tests.

Official/reference material:

- Semtech SX1280 product page: https://www.semtech.com/products/wireless-rf/lora-connect/sx1280
- LILYGO T3-S3 MVSR repository: https://github.com/Xinyuan-LilyGO/T3-S3-MVSRBoard
- RadioLib SX1280 API: https://jgromes.github.io/RadioLib/class_s_x1280.html

## 4. Raw audio budget

For mono 16-bit PCM:

```text
bytes/frame = sample_rate * 2 * frame_duration_seconds
```

| Audio format | Raw bytes / 10 ms | FLRC fragments with 10 B header | GFSK fragments with 10 B header |
|---|---:|---:|---:|
| 16 kHz / 16-bit / mono | 320 B | 3 | 2 |
| 24 kHz / 16-bit / mono | 480 B | 5 | 2 |
| 32 kHz / 16-bit / mono | 640 B | 6 | 3 |
| 48 kHz / 16-bit / mono | 960 B | 9 | 4 |

Fragmentation is implemented because it is required for general data frames,
but sending raw high-rate PCM as many RF packets is not the preferred final
audio design.

Run the reproducible calculator:

```bash
python tools/rf_packet_budget.py
```

## 5. Audio codec gates

Codec selection is deliberately staged. We must not choose a complex codec
before the RF path itself works.

### Gate A — no audio codec

Use deterministic 60-100 byte synthetic payloads.

Purpose:

- validate SX1280 TX/RX
- packet sequence tracking
- packet error/loss measurement
- latency measurement
- one-to-one baseline

### Gate B — raw PCM + fragmentation

Use a short known PCM sample only to prove that multi-packet reassembly is
bit-exact.

This is a protocol test, not the intended continuous-audio transport.

### Gate C — low-complexity PoC codec candidate

First candidate: **16 kHz mono IMA-ADPCM, 10 ms blocks**.

Reason for testing it:

- 16 kHz PCM frame = 320 B / 10 ms
- 4-bit ADPCM is approximately one quarter of the PCM size
- estimated coded data ~= 80 B / 10 ms
- allowing a few bytes of block state gives an approximately 84 B frame
- 84 B + 10 B PR1 header fits inside one 127 B FLRC packet

This is a **PoC candidate, not a final sound-quality decision**. ADPCM may be
acceptable for speech/link validation while being inadequate for the eventual
music experience.

### Gate D — final-quality codec experiment

Only after the radio link is stable, compare higher quality low-bitrate codec
options using real music and speech. Opus is one candidate because low-bitrate
encoded frames can be much smaller than raw PCM, but embedded CPU/RAM cost,
frame timing, implementation availability, licensing obligations, and audible
quality must be verified before selection.

No final codec is locked yet.

## 6. PR1 RF protocol v2

The application header is intentionally small:

| Offset | Bytes | Field | Meaning |
|---:|---:|---|---|
| 0 | 2 | magic | ASCII `P1` |
| 2 | 1 | version | `2` |
| 3 | 1 | flags | codec / future state bits |
| 4 | 2 | packet_seq | sequence per RF packet |
| 6 | 2 | frame_seq | sequence per audio/application frame |
| 8 | 1 | fragment_index | zero-based fragment number |
| 9 | 1 | fragment_count | fragments in this frame |

Total: **10 bytes**.

Payload length is inferred from the actual received RF packet length. This saves
bytes compared with carrying a redundant payload-length field.

Current implementation:

- `tools/pr1_rf_protocol.py`
- `tools/test_pr1_rf_protocol.py`

### Why both packet_seq and frame_seq exist

`packet_seq` detects lost or reordered RF packets.

`frame_seq` lets the receiver know which fragments belong to one audio frame and
is the future jitter-buffer ordering key.

Both are 16-bit and wrap naturally. Wrap-safe ordering logic must be added to
firmware before long-duration tests.

## 7. Fragmentation rules

1. `max_rf_payload` includes the 10-byte PR1 header.
2. A frame is split into chunks of `max_rf_payload - 10` bytes.
3. Every fragment repeats the same `frame_seq` and `fragment_count`.
4. `fragment_index` starts at zero.
5. `packet_seq` increments for every transmitted RF packet.
6. Duplicates are ignored by the reassembler.
7. Out-of-order fragments may still complete a frame.
8. Incomplete frames must expire rather than block playback forever.

The PC reassembler already bounds the number of in-flight frames. Firmware must
add an explicit time deadline tied to the audio playback schedule.

## 8. Hardware bring-up order

Do not begin with live microphone audio.

1. Photograph and identify the exact board revision and `Without PA` radio
   version.
2. Confirm antenna/connector configuration from the physical board and official
   schematic.
3. Flash the vendor SX1280 PingPong example unchanged.
4. At a legal/approved bench-test configuration, verify short-range packet TX/RX.
5. Log packet count, errors, RSSI/SNR where available, mode and radio settings.
6. Send PR1 deterministic 60-100 byte packets.
7. Validate fragmentation/reassembly using known bytes.
8. Validate MAX98357A playback separately from microSD.
9. Join known stored audio -> codec -> RF -> decode -> playback.
10. Only after this works should microphone/live-source work begin.

Ambient microphone capture is not required for PR1's core customer value and
should not distract the first PoC.

## 9. Acceptance gates

### P0 — PC protocol

- header unit tests pass
- 640 B frame survives fragmentation/reassembly exactly
- duplicate and out-of-order fragments handled
- packet-budget calculator documents every selected format

### P1 — vendor RF baseline

- both exact boards identified
- vendor SX1280 PingPong works on both boards
- no PA-version mismatch
- radio settings recorded, not guessed

### P2 — PR1 data link

- at least 10,000 deterministic packets tested at bench range
- packet loss/error metrics recorded
- no silent corruption
- test conditions recorded

### P3 — stored-audio link

- known audio plays source -> RF -> receiver without live microphone dependency
- end-to-end latency measured
- underruns, dropped frames and packet loss are separately logged

### P4 — quality / distance trade-off

- compare at least three codec/bitrate or radio-setting combinations
- do not optimize distance by silently violating permitted operating conditions
- choose a design only from measured results

## 10. Legal and certification gate

The current LILYGO boards are development hardware. A successful bench PoC does
not prove that an open-air configuration is lawful and does not make the board
a sale-ready Korean product.

Before outdoor RF testing:

- confirm the exact Korean technical rule that applies to the selected SX1280
  mode, output power, antenna and bandwidth
- confirm whether the exact equipment/configuration is covered by conformity
  assessment, an exemption, or requires an experimental-radio-station route
- use the lowest useful power during controlled testing
- obtain venue permission where installation/use requires it

Before sale:

- freeze the production RF, antenna, power, battery and enclosure design
- use a certification-friendly radio architecture
- complete the applicable Korean conformity/safety procedures for the final
  product

The project must treat certification as a production design input, not as a
sticker added after engineering is finished.

## 11. What is deliberately NOT locked yet

- FLRC vs GFSK vs another usable SX1280 mode for the final PoC
- final RF bitrate, coding rate, frequency and power
- final codec
- final sample rate
- final antenna
- final product MCU/radio module
- custom PCB layout
- one-to-many transport behavior

Those decisions require measured evidence or regulatory confirmation first.

## 12. Immediate next task

After the two boards physically arrive:

**Run the vendor SX1280 PingPong baseline first. Do not start audio debugging
until both boards can exchange deterministic packets reliably.**
