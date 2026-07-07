# Running Audio — PR1

Low-cost wireless audio prototype for listening to PC playback through a separate receiver and one bone-conduction transducer.

## Active prototype

`PC playback -> USB-C -> LILYGO T3-S3 SX1280 TX -> FLRC -> LILYGO T3-S3 SX1280 RX -> MAX98357A -> transducer`

The former ESP32-S3 Wi-Fi/UDP design is superseded and remains only as historical work in its closed draft pull request.

## Fixed first-build constraints

- Budget ceiling: KRW 150,000
- Audio: 16 kHz, signed 16-bit, mono
- Boards: two LILYGO T3-S3 SX1280
- Radio: SX1280 FLRC
- Output: one MAX98357A and one bone-conduction transducer
- Development: PlatformIO, Arduino framework, RadioLib
- First test power: USB only

## Build

```bash
python -m pip install platformio
pio run -e tx
pio run -e rx
```

See:

- `docs/prototype_v2.md`
- `docs/wiring_v2.md`
- `docs/bom_v2.md`
- `docs/test_procedure_v2.md`

No physical test result is claimed until both boards are flashed and the documented test is run.
