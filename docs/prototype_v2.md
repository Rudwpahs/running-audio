# PR1 Prototype v2 — SX1280 FLRC PoC

## Binding decision

The first physical prototype is no longer the ESP32-S3 Wi-Fi/UDP design.

The active path is:

`PC playback -> USB-C -> LILYGO T3-S3 SX1280 TX -> SX1280 FLRC -> LILYGO T3-S3 SX1280 RX -> MAX98357A -> one bone-conduction transducer`

The previous Wi-Fi/UDP firmware remains historical work only and must not be used as the purchasing or implementation baseline.

## Fixed v2 scope

- Total prototype budget ceiling: KRW 150,000
- Two LILYGO T3-S3 SX1280 boards
- Two USB-C data cables
- One MAX98357A amplifier module
- One bone-conduction transducer
- One simple band or clip mount
- No battery in the first bench test
- No microphone or PCM1808 in the first prototype
- No external Wi-Fi, router, or internet dependency
- One transmitter and one receiver only

## Audio and packet format

- PCM: signed 16-bit little-endian
- Sample rate: 16 kHz
- Channels: mono
- Samples per radio packet: 120
- Audio duration per packet: 7.5 ms
- PCM payload: 240 bytes
- Radio header: 12 bytes
- Total radio packet: 252 bytes
- Receiver startup jitter depth: 6 packets / 45 ms
- Receiver queue capacity: 16 packets

| Offset | Size | Field |
|---:|---:|---|
| 0 | 2 | Magic `P2` |
| 2 | 1 | Version `0x02` |
| 3 | 1 | Flags, audio=`0x01` |
| 4 | 4 | Sequence, big-endian |
| 8 | 4 | Sample index, big-endian |
| 12 | 240 | PCM samples |

## Initial FLRC configuration

- Frequency: 2410.5 MHz
- Bit rate: 520 kbps
- Coding rate: 2
- Output power: 3 dBm
- Data shaping: 1.0
- Sync word: `50 52 31 32` (`PR12`)
- No application-level retry in the first audio test

## Software structure

- `host/pc_audio_sender.py`: PC playback capture, 16 kHz mono conversion, USB framing
- `src/tx_role.cpp`: USB frame parser and FLRC transmitter
- `src/rx_role.cpp`: packet validation, jitter queue, gap concealment, I2S output
- `include/pr1_protocol.h`: shared packet codec
- `include/pr1_config.h`: locked pins and radio/audio parameters

## Exit criteria

The v2 PoC is successful only after a human test confirms:

1. Both firmware environments build and flash.
2. The TX receives continuous PC audio over USB.
3. The RX outputs recognizable audio through the transducer.
4. Ten minutes of playback complete without a board reset.
5. Packet loss, queue overflow, and late-packet counts are recorded.
6. Output begins at the software-limited level and is comfortable.
