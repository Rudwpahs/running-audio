# PR1 SX1280 Implementation Notes

Date: 2026-07-23
Status: **implementation supplement, not a new product pivot**

## 0. Document and protocol hierarchy

When documents disagree, use this order:

1. `docs/DECISIONS_2026-07-23.md` — product/customer decisions
2. `docs/TECHNICAL_MVP.md` — technical stage gates and voice v0 profile
3. `firmware/common/pr1_packet.hpp` — current canonical application-packet codec
4. this document — SX1280 RF transport/firmware implementation details
5. experiment logs — measured evidence that may trigger a documented revision

Current product direction remains **voice-first, phone-free, one-to-many,
open-ear group audio**. This file does not restore the older Hi-Fi/music-first
scope.

Two layers now have deliberately different jobs:

```text
voice/application layer
  pr1_packet.hpp
  16 B application header + 160 B voice v0 = 176 B
                 ↓ serialized application packet
RF transport layer
  pr1_rf_protocol.h / pr1_rf_protocol.py
  10 B transport envelope + fragmentation metadata
                 ↓
SX1280 packet mode
```

The 10-byte transport header does **not** replace the 16-byte application
header.

---

## 1. Current first-PoC hardware

Ordered first prototype:

- 2 x LILYGO T3-S3 MVSR
- radio option: `SX1280 2.4G Without PA + MVSR [H594-01]`
- onboard MAX98357A speaker amplifier
- microSD for the first controlled prerecorded source
- bone-conduction transducer after connector/output suitability is verified

The LILYGO board is a development vehicle, not the locked production PCB.

### Vendor SX1280 pins used by M0

From the MVSR vendor `pin_config.h` SX1280 branch:

| Function | GPIO |
|---|---:|
| SCLK | 5 |
| MISO | 3 |
| MOSI | 6 |
| CS | 7 |
| RST | 8 |
| DIO1 | 9 |
| BUSY | 36 |
| RF TX enable | 10 |
| RF RX enable | 21 |

Important: the vendor `pin_config.h` can have a different radio model selected
by default. Do not copy the active preprocessor selection blindly; identify the
physical board first.

References:

- https://github.com/Xinyuan-LilyGO/T3-S3-MVSRBoard
- https://github.com/Xinyuan-LILYGO/LilyGo-LoRa-Series
- https://jgromes.github.io/RadioLib/class_s_x1280.html

---

## 2. Current voice profile stays simple

`TECHNICAL_MVP.md` currently starts audio with:

- 8 kHz
- unsigned PCM 8-bit
- mono
- 20 ms frame
- 160 audio bytes/frame
- 50 audio frames/s
- 64 kbps raw audio

The canonical application packet adds its 16-byte header, producing a **176-byte
serialized voice v0 application packet**.

Do **not** add ADPCM/Opus before the raw path and modulation limits have been
measured. Compression becomes a later experiment only when evidence shows the
raw profile needs more margin.

---

## 3. Packet-size reality by radio mode

The implementation tools currently use these SX1280 payload ceilings for packet
budgeting:

| Packet mode | Payload ceiling used by tools |
|---|---:|
| FLRC | 127 B |
| LoRa / GFSK | 255 B |

These are packet-format budgets, not a statement that a particular mode,
frequency, power or antenna configuration is lawful for open-air use in Korea.

The 10-byte RF transport envelope leaves:

- 117 bytes of transport payload under a 127-byte ceiling
- 245 bytes of transport payload under a 255-byte ceiling

Therefore:

| Transport payload | 127-B ceiling | 255-B ceiling |
|---|---:|---:|
| M0 deterministic payload: 90 B | 1 RF packet | 1 RF packet |
| Voice v0 canonical app packet: 176 B | 2 RF packets | 1 RF packet |
| Old 32 kHz PCM + 16-B app header: 656 B | 6 RF packets | 3 RF packets |

Run the calculator instead of hand-copying the table:

```bash
python tools/rf_packet_budget.py
```

---

## 4. RF transport v2

The lower layer currently implements this 10-byte transport envelope:

| Offset | Bytes | Field | Purpose |
|---:|---:|---|---|
| 0 | 2 | magic | ASCII `P1` |
| 2 | 1 | transport_version | `2` |
| 3 | 1 | flags | transport/test/future state |
| 4 | 2 | packet_seq | sequence per physical RF packet |
| 6 | 2 | frame_seq | groups fragments of one upper-layer frame |
| 8 | 1 | fragment_index | zero-based fragment index |
| 9 | 1 | fragment_count | fragments in the upper-layer frame |

C++ types live in `namespace pr1::rf` specifically so they can coexist with the
canonical application-layer `pr1::Header` from `pr1_packet.hpp`.

### T2 wrapping rule

When prerecorded voice is implemented:

1. build the 176-byte application packet using `pr1_packet.hpp`
2. treat those serialized bytes as one RF-transport frame
3. set transport `frame_seq` to the same logical sequence as the application
   packet where practical
4. fragment according to the selected SX1280 packet-mode ceiling
5. transmit each fragment with its own `packet_seq`
6. receiver reassembles the transport frame
7. only after complete reassembly, decode the canonical application packet

This keeps the audio/application format identical across LoRa, FLRC and GFSK;
only the lower RF fragmentation behavior changes.

M0 is intentionally simpler: it uses the RF transport directly around a
90-byte deterministic pattern and does not create an application voice packet.

---

## 5. Fragmentation/reassembly behavior

PC implementation:

- `tools/pr1_rf_protocol.py`
- `tools/test_pr1_rf_protocol.py`

Portable C++ implementation:

- `firmware/common/pr1_rf_protocol.h`
- `firmware/host_tests/test_pr1_rf_protocol.cpp`

Rules:

1. `max_rf_payload` includes the 10-byte transport header.
2. `packet_seq` increments for every RF packet.
3. `frame_seq` identifies all fragments belonging to one upper-layer frame.
4. fragments may arrive out of order.
5. duplicate fragments before completion are ignored.
6. recently completed frame sequences are remembered for a bounded window so a
   duplicate whole frame is not emitted twice.
7. incomplete frames must eventually expire in real firmware.
8. sequence history is bounded so uint16 wrap/reuse remains possible.

Fragmentation exists even though M0 fits in one packet, because the canonical
176-byte voice packet exceeds a 127-byte FLRC packet budget.

---

## 6. M0 firmware — deterministic data only

Path:

`firmware/m0_sx1280_packet_link/`

M0 answers one question:

> Can the two exact MVSR/SX1280 boards exchange measurable 100-byte transport
> packets without silent corruption?

It deliberately excludes:

- microphone
- microSD audio streaming
- application voice packet
- codec
- jitter buffer
- UI
- one-to-many fleet behavior

### M0 packet

```text
10 B RF transport header
90 B deterministic pattern
----------------------------
100 B total RF packet
```

The payload pattern is derived from `packet_seq`, allowing the receiver to
separate:

- radio receive error
- invalid transport header
- valid header + corrupt payload bytes
- valid packet

### Safe build

The checked-in default sets `PR1_ENABLE_RF_TEST=0`.

No guessed frequency or output-power value is checked in. A live RF build
requires explicit local compile-time values after the exact bench-test route and
conditions are confirmed.

### Initial M0 modulation

M0 currently uses FLRC 650 kbps as **one initial data-link baseline**, not a
final modulation decision. `TECHNICAL_MVP.md` still requires measured comparison
with other relevant SX1280 modes.

---

## 7. Required hardware order

### P1 — physical inspection

- photograph both boards
- confirm SX1280 **Without PA** on both
- record revision markings
- inspect antenna and connector
- confirm speaker connector before connecting transducer

### P2 — unmodified vendor baseline

Before PR1 firmware:

- flash the vendor SX1280 PingPong example
- record upstream commit/example
- record actual radio settings
- prove A→B and B→A short-packet communication under the confirmed test setup

### P3 — M0 safe build

- PlatformIO compile
- flash `safe`
- verify printed pins
- verify `rf_enabled=0`

### P4 — controlled M0 link

- enable RF only with explicit reviewed test settings
- start with 100 packets
- then run the T1 gate count
- preserve full TX/RX logs

The current `TECHNICAL_MVP.md` T1 minimum is 1,000 packets at 20 m LOS. The
analysis tools also support a larger 10,000-packet bench run, but a larger packet
count does not replace the prescribed environment/distance test.

---

## 8. Log analysis

RX-only diagnosis:

```bash
python tools/analyze_m0_serial.py rx.log
```

Controlled TX/RX loss calculation:

```bash
python tools/analyze_m0_run.py tx.log rx.log
```

The second tool compares successful TX sequence numbers with valid RX sequence
numbers, so losses before the receiver's first observed packet are not silently
missed.

It reports:

- TX failures
- matched valid packets
- missing after successful TX
- duplicate RX
- unexpected/stale RX
- bad transport header
- bad deterministic payload pattern
- radio errors

Keep one controlled M0 run below 65,536 successful TX packets so the uint16
sequence value is unique within that run.

---

## 9. Verification

Existing main CI validates the canonical application packet in both Python and
portable C++. This integration adds independent lower-layer RF transport tests.

Run all checks available in the merged tree with:

```bash
bash scripts/ci.sh
```

The Arduino/RadioLib M0 project still requires a real PlatformIO build. The agent
environment used for this integration did not have PlatformIO installed, so no
board-firmware compile success is claimed yet.

---

## 10. Productization boundary

For Korean regulatory/productization decisions, use:

`docs/REGULATORY_GATE.md`

A successful LILYGO PoC does not make the dev board the final product. After
buyer + T3/T5 evidence, freeze the production radio, antenna, PCB, battery,
charger, firmware limits and enclosure, then use the applicable RRA/designated
laboratory conformity/safety route.

Do not spend the early budget certifying the development board as though it were
the final sale configuration.
