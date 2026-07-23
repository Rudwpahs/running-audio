# PR1 SX1280 Implementation Notes

Date: 2026-07-23
Status: **implementation supplement, not a new product pivot**

## 0. Document hierarchy

When documents disagree, use this order:

1. `docs/DECISIONS_2026-07-23.md` — product/customer decisions
2. `docs/TECHNICAL_MVP.md` — technical stage gates and current voice v0 profile
3. this document — concrete SX1280 transport/firmware implementation details
4. individual experiment logs — measured evidence that may trigger a documented revision

The current product direction remains **voice-first, phone-free, one-to-many,
open-ear group audio**. This file does not restore the older Hi-Fi/music-first
scope.

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
- https://github.com/Xinyuan-LilyGO/LilyGo-LoRa-Series
- https://jgromes.github.io/RadioLib/class_s_x1280.html

---

## 2. Current voice profile stays simple

`TECHNICAL_MVP.md` currently starts audio with:

- 8 kHz
- unsigned PCM 8-bit
- mono
- 20 ms frame
- 160 application audio bytes/frame
- 50 audio frames/s
- 64 kbps raw audio

Do **not** add ADPCM/Opus before the raw path and modulation limits have been
measured.

Compression becomes a later experiment only when evidence shows the raw profile
needs more margin.

---

## 3. Packet-size reality by radio mode

The implementation tools use these SX1280 payload ceilings for budgeting:

| Packet mode | Payload ceiling used by tools |
|---|---:|
| FLRC | 127 B |
| LoRa / GFSK | 255 B |

These are packet-format budgets, not a statement that a particular mode,
frequency, power or antenna configuration is lawful for an open-air test in
Korea.

With the current 10-byte compact transport header:

| Application frame | 127-B ceiling | 255-B ceiling |
|---|---:|---:|
| T1 deterministic data: 90 B | 1 packet | 1 packet |
| Voice v0 raw PCM: 160 B | 2 packets | 1 packet |
| Old 32 kHz/16-bit/10 ms PCM: 640 B | 6 packets | 3 packets |

Run the calculator rather than hand-copying the table:

```bash
python tools/rf_packet_budget.py
```

---

## 4. Compact PR1 transport v2 — M0/T1 implementation

The code currently implements this 10-byte header:

| Offset | Bytes | Field | Purpose |
|---:|---:|---|---|
| 0 | 2 | magic | ASCII `P1` |
| 2 | 1 | version | `2` |
| 3 | 1 | flags | test/codec/future bits |
| 4 | 2 | packet_seq | RF packet loss/order |
| 6 | 2 | frame_seq | application/audio frame grouping/order |
| 8 | 1 | fragment_index | zero-based fragment index |
| 9 | 1 | fragment_count | fragments in the frame |

Why it is smaller than the 16-byte `Application Packet v1` proposal in
`TECHNICAL_MVP.md`:

- payload length can be inferred from the received packet length
- sample rate is fixed by the active test profile
- M0 has one stream, so a stream identifier is not yet required
- a TX clock timestamp cannot by itself provide true one-way latency unless the
  clocks are synchronized; latency needs an explicit measurement method
- fragmentation fields are useful when comparing 127-B and 255-B packet modes

### Protocol decision gate before T2

Do **not** maintain two production packet formats.

After T1 measurements, before prerecorded audio T2:

1. decide whether the compact v2 header becomes the audio packet header, or
2. adopt a revised header that restores fields such as `stream_id` if the
   measured/operational need justifies the bytes.

Record that decision in `DECISIONS_2026-07-23.md` or a dated successor.

---

## 5. Fragmentation/reassembly behavior

PC implementation:

- `tools/pr1_rf_protocol.py`
- `tools/test_pr1_rf_protocol.py`

Portable C++ implementation:

- `firmware/common/pr1_rf_protocol.h`
- `firmware/host_tests/test_pr1_rf_protocol.cpp`

Rules:

1. `max_rf_payload` includes the PR1 header.
2. `packet_seq` increments for every RF packet.
3. `frame_seq` identifies all fragments belonging to one application frame.
4. fragments may arrive out of order.
5. duplicate fragments before completion are ignored.
6. recently completed frame sequences are remembered for a bounded window so a
   duplicate whole frame is not emitted twice.
7. incomplete frames must eventually expire in real firmware.
8. sequence history is bounded so uint16 wrap/reuse remains possible.

The PC code intentionally handles fragmentation even though the first T1 packet
fits in one packet. That prevents another architecture rewrite when a later mode
or audio profile needs multiple packets.

---

## 6. M0 firmware — deterministic data only

Path:

`firmware/m0_sx1280_packet_link/`

M0 answers one question:

> Can the two exact MVSR/SX1280 boards exchange measurable 100-byte PR1 data
> packets without silent corruption?

It deliberately excludes:

- microphone
- microSD audio streaming
- codec
- jitter buffer
- UI
- one-to-many fleet behavior

### M0 packet

```text
10 B compact header
90 B deterministic pattern
-------------------------
100 B total
```

The payload pattern is derived from `packet_seq`, allowing the receiver to
separate:

- radio receive error
- invalid PR1 header
- valid header + corrupt application bytes
- valid packet

### Safe build

The checked-in default sets `PR1_ENABLE_RF_TEST=0`.

No checked-in guessed frequency or output-power value exists. A live RF build
requires explicit local compile-time values after the exact bench-test route and
conditions are confirmed.

### Initial M0 modulation

M0 currently uses FLRC 650 kbps as **one initial data-link baseline**, not a
final modulation decision. `TECHNICAL_MVP.md` still requires comparison with
other SX1280 modes using measurements.

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
analysis tools support a larger 10,000-packet bench run when useful, but a larger
number does not replace the prescribed environment/distance test.

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

It also reports:

- TX failures
- matched valid packets
- missing after successful TX
- duplicate RX
- unexpected/stale RX
- bad header
- bad payload pattern
- radio errors

Keep one controlled M0 run below 65,536 successful TX packets so the uint16
sequence value is unique within that run.

---

## 9. Local verification

Run:

```bash
bash scripts/ci.sh
```

This combines:

- the existing v0 packet simulator from main
- compact protocol Python tests
- M0 log-analysis tests
- packet-budget calculator
- portable C++ protocol compile with strict warnings

The Arduino/RadioLib project still requires a real PlatformIO build; the agent
environment used for this integration did not have PlatformIO installed.

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
