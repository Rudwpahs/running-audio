# PR1 Radio Architecture Options

Status: **Do not choose final architecture yet.**

The current SX1280 hardware is the PoC vehicle, not an irreversible product decision.

## Option A — SX1280 proprietary broadcast

### Strengths
- current hardware already selected/purchased for PoC
- 2.4 GHz global ISM family
- LoRa / FLRC / GFSK available in the chip
- full control of packet format, receiver behavior and latency experiments
- participant smartphone not required

### Weaknesses
- PR1 owns audio transport, buffering, recovery, security and fleet protocol complexity
- proprietary receiver ecosystem
- future interoperability is limited
- final certification/manufacturing architecture must be designed deliberately

### Use now?
**YES — PoC.**

### Commit as final product?
**NO — not before field + buyer validation.**

---

## Option B — Bluetooth LE Audio / Auracast

### Strengths
- standardized broadcast audio
- LC3 audio ecosystem
- designed for one broadcaster to many in-range receivers
- public-location / fitness / guided-audio use cases are part of the ecosystem direction
- potential interoperability with future consumer devices

### Weaknesses / open questions
- receiver compatibility and user joining UX must be tested with actual devices
- product certification/licensing/Bluetooth qualification path needs review
- current PR1 dev boards are not a drop-in Auracast development platform
- buying a new LE Audio development platform now would distract from the first PoC

### Use now?
**Research only. Do not buy another dev kit before T3 + buyer evidence.**

---

## Option C — OEM / existing group-audio platform

### Strengths
- fastest path to a commercial pilot
- proven range/battery/charging ecosystem may already exist
- reduces RF engineering burden

### Weaknesses
- low technical moat
- supplier dependence
- difficult to create a unique compact open-ear experience unless customized

### Use now?
Keep as a fallback and as a benchmark.

If customers strongly want the operational experience but do not care about PR1's radio technology, OEM can be the more rational business path.

---

# Decision Matrix after T3/T5

Score 1–5 using measured facts.

| Criterion | Weight | SX1280 | Auracast | OEM |
|---|---:|---:|---:|---:|
| voice reliability in target field | 20 | | | |
| latency | 10 | | | |
| receiver size/power | 10 | | | |
| one-to-many scalability | 10 | | | |
| certification/productization risk | 10 | | | |
| BOM cost | 10 | | | |
| development time | 10 | | | |
| interoperability | 5 | | | |
| defensibility/customization | 10 | | | |
| supply-chain risk | 5 | | | |
| **Weighted total** | **100** | | | |

Do not fill scores from intuition. Each score needs a measurement, quote, supplier evidence, or official specification.

---

# Architecture Review Trigger

Hold the first formal review only after:

1. SX1280 live voice T3 result exists.
2. 50m outdoor T5 result exists.
3. 10 buyer interviews are complete.
4. At least one real pilot is scheduled.

Until then, architecture comparison is research, not a reason to restart the project.

---

# Sources
- Semtech SX1280: https://www.semtech.com/products/wireless-rf/lora-connect/sx1280
- LILYGO MVSR: https://wiki.lilygo.cc/products/t3-series/t3-s3-mvsr/
- Bluetooth Auracast: https://www.bluetooth.com/auracast/
