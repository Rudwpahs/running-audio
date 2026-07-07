# PR1 AI Handoff — Prototype v2

> Repository: `Rudwpahs/running-audio`
>
> Active branch: `agent/sx1280-flrc-poc`
>
> **Prototype v1 is superseded.** The former ESP32-S3 Wi-Fi/UDP and PCM1808 specification must not be used for purchasing or new implementation work.

## Roles

### GPT

- Owns architecture, hardware, radio parameters, packet format, pins, acceptance criteria, implementation, and final review.

### Claude

- Optional focused reviewer when credits are available.
- Must not independently change the locked v2 hardware, radio, packet, audio, or pin specification.

## Locked prototype v2

### Product path

`PC playback -> USB-C -> LILYGO T3-S3 SX1280 TX -> SX1280 FLRC -> LILYGO T3-S3 SX1280 RX -> MAX98357A -> one bone-conduction transducer`

### Cost and hardware

- Total purchase ceiling: KRW 150,000
- TX board: LILYGO T3-S3 SX1280
- RX board: LILYGO T3-S3 SX1280
- Two USB-C data cables
- One MAX98357A
- One bone-conduction transducer
- One simple band or clip mount
- No separate ESP32 development boards
- No PCM1808, microphone, or battery in the first bench build

### Development environment

- PlatformIO
- Arduino framework
- Platform: `espressif32@6.13.0`
- Board definition: `t3_s3_v1_x`
- Radio library: RadioLib `7.7.1`
- Environments: `tx`, `rx`

### Audio

- PCM: signed 16-bit little-endian
- Sample rate: 16 kHz
- Channels: mono
- Samples per packet: 120
- Packet duration: 7.5 ms
- PCM payload: 240 bytes
- Receiver startup jitter: 6 packets / 45 ms
- Queue capacity: 16 packets
- Receiver software limiter: absolute sample value 4096

### Radio

- SX1280 FLRC
- Frequency: 2410.5 MHz
- Bit rate: 520 kbps
- Coding rate: 2
- Output power: 3 dBm
- Data shaping: 1.0
- Sync word: `50 52 31 32`
- No application retry in the first audio PoC

### Radio pins

- SCK: GPIO5
- MISO: GPIO3
- MOSI: GPIO6
- CS: GPIO7
- RESET: GPIO8
- DIO1: GPIO9
- BUSY: GPIO36
- Status LED: GPIO37

### Receiver I2S pins

- BCLK: GPIO43
- LRCLK/WS: GPIO44
- DATA: GPIO21

### Radio packet

Total: 252 bytes.

| Offset | Size | Field |
|---:|---:|---|
| 0 | 2 | Magic `P2` |
| 2 | 1 | Version `0x02` |
| 3 | 1 | Flags, audio=`0x01` |
| 4 | 4 | Sequence, big-endian |
| 8 | 4 | Sample index, big-endian |
| 12 | 240 | PCM samples |

### PC-to-TX USB frame

- Magic: `PR1U`, 4 bytes
- Sequence: uint32 little-endian
- PCM: 240 bytes
- Total: 248 bytes
- Serial/USB CDC setting: 921600 baud

## Current state

- Old PR #1 and PR #2 are closed as superseded.
- v2 documentation, TX firmware, RX firmware, host sender, packet test, and CI are being implemented on `agent/sx1280-flrc-poc`.
- No physical hardware test has been performed.
- No audio-output success may be claimed until a human flashes both boards and runs `docs/test_procedure_v2.md`.

## Required validation

1. `pio run -e tx`
2. `pio run -e rx`
3. Python syntax check for `host/pc_audio_sender.py`
4. Native C++ protocol test
5. TX and RX flash
6. 1 m / 2 minute bench audio test
7. 5 m / 10 minute room test
8. Record received, invalid, queue-drop, missing, and late counts

## Safety rules

- Attach an SX1280 antenna before any transmission.
- Use USB power only for the first test.
- Keep the transducer off the head during first power-on.
- Begin with low PC volume.
- Stop if the amplifier, transducer, or board becomes unusually hot.
