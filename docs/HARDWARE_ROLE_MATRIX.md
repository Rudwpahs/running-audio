# PR1 Current Hardware Role Matrix

Status: 2026-07-23

This document prevents an avoidable mistake: assuming that two SX1280 boards automatically mean two complete audio endpoints.

## Known radio variants

The current PR1 plan uses:

- **H594** — T3-S3, SX1280 2.4 GHz **without PA**
- **H594-01** — T3-S3 + MVSR, SX1280 2.4 GHz **without PA**

Current product references:
- https://lilygo.cc/products/t3s3-v1-0
- https://wiki.lilygo.cc/products/t3-series/t3-s3-mvsr/

The MVSR expansion adds:
- digital microphone: I2S on V1.0 / PDM on V1.1
- MAX98357A I2S speaker amplifier
- RTC
- vibration motor
- audio/voice examples

The plain H594 T3-S3 is still a useful radio/MCU/TF-card endpoint, but it is not automatically a complete microphone + speaker endpoint.

---

# What the two known boards can prove immediately

## T0 — board/radio bring-up

| Test | H594 | H594-01 MVSR | Additional hardware required? |
|---|---|---|---|
| USB serial | yes | yes | no |
| TF/microSD | yes | yes | no |
| SX1280 TX/RX | yes | yes | no |
| Microphone capture | not assumed | yes | possibly on H594 |
| Speaker/I2S amp output | not assumed | yes | possibly on H594 |

Do not wire an external audio device until the actual delivered accessories are inventoried.

---

## T1 — RF data 1→1

**Can proceed with the two boards alone.**

Suggested roles:
- H594 = transmitter
- H594-01 = receiver

Then reverse the roles once to ensure both radios work in both directions.

No audio code is needed for T1.

---

## T2 — prerecorded voice 1→1

**Can proceed with the two boards alone.**

Recommended arrangement:

```text
H594 plain board
microSD / memory speech frame
        ↓
SX1280 TX
        )))) 2.4 GHz ((((
SX1280 RX
        ↓
H594-01 MVSR
MAX98357A speaker output
```

This is the best first audio test because it separates RF/audio transport from microphone capture.

T2 should happen before any live-microphone work.

---

# T3 — live microphone 1→1

A live link requires **both ends of the chain**:

```text
MIC → TX MCU → TX RADIO → RX RADIO → RX MCU → AUDIO OUT
```

The H594-01 MVSR provides both MIC and AUDIO OUT, but one physical board can only occupy one endpoint in the two-board radio link.

Therefore, with one plain H594 + one H594-01 MVSR:

### Option A — MVSR is transmitter
- MVSR: microphone capture ✅
- H594: RF receive ✅
- H594: audio output ❓ requires an external audio output peripheral if none was already purchased

### Option B — MVSR is receiver
- H594: RF transmit ✅
- H594: microphone capture ❓ requires an external microphone peripheral if none was already purchased
- MVSR: audio output ✅

### Conclusion

**A third SX1280 board is not required for T3.**

But the plain H594 side needs one missing audio direction unless the existing order already includes the relevant external peripheral.

Do not purchase anything yet. Inventory the delivered parts first.

---

# Delivery-day inventory gate

Before ordering another part, photograph/record the actual items and answer:

1. Is the main board label H594?
2. Is the MVSR kit label H594-01?
3. MVSR board version: V1.0 or V1.1?
   - V1.0 mic = MSM261S4030H0R I2S
   - V1.1 mic = MP34DT05-A PDM
4. Is there an external MAX98357A module in the existing order?
5. Is there an external I2S/PDM microphone module in the existing order?
6. What exact speaker / bone-conduction transducer arrived?
7. What impedance and rated power are marked on it?
8. Do both SX1280 boards have the correct 2.4 GHz antenna connected before transmitting?

Only after this inventory decide whether T3 needs one small additional audio module.

---

# Official MVSR pins — reference, not a wiring instruction yet

From LILYGO MVSR documentation:

### Speaker MAX98357A
- BCLK GPIO40
- LRCLK GPIO41
- DATA GPIO39
- SD_MODE GPIO38

### Mic V1.0 I2S
- BCLK GPIO47
- WS GPIO15
- DATA GPIO48
- EN GPIO35

### Mic V1.1 PDM
- LRCLK GPIO15
- DATA GPIO48
- EN GPIO35

### SX1280
- CS GPIO7
- RST GPIO8
- SCLK GPIO5
- MOSI GPIO6
- MISO GPIO3
- DIO1 GPIO9
- BUSY GPIO36
- TX GPIO10
- RX GPIO21

Source:
https://wiki.lilygo.cc/products/t3-series/t3-s3-mvsr/

Do not assume the plain H594 exposes the same MVSR audio circuitry; it does not include the MVSR expansion simply because the MCU/radio family is similar.

---

# Correct development order

1. Inventory exact delivered hardware.
2. Run official board/radio examples.
3. T1 RF packet measurement.
4. T2 prerecorded voice using H594 → H594-01.
5. Decide the cheapest complementary audio peripheral for H594, **only if required**.
6. T3 live microphone.
7. Outdoor 20/50/100m matrix.

This order avoids buying a third radio board just to solve an audio-interface problem.
