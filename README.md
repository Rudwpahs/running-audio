# PR1 — Screen-free Group Audio

PR1 is LUN's current prototype project: a **phone-free, one-to-many wireless audio system for outdoor education, camps and sports coaching**.

The current goal is not to build a consumer earbud or a park-wide Hi-Fi network. The goal is to prove three things in order:

1. a buyer has a recurring problem,
2. live voice works reliably in the target outdoor scenario,
3. a buyer will commit to a paid pilot.

## Current status — 2026-07-23

- Beachhead hypothesis: **youth camps / outdoor experiential education**
- Second segment: **running / sports coaching**
- PoC radio: **LILYGO T3-S3 + SX1280/MVSR**
- Known radio variants: **H594 plain + H594-01 MVSR, both SX1280 without PA**
- v0 audio profile: **8 kHz / 8-bit / mono / 20 ms (160-byte audio frame)**
- v0 application packet: **176 bytes total**
- Final radio architecture: **not decided** — SX1280, Bluetooth LE Audio/Auracast and OEM routes will be compared after field and buyer evidence.

## Start here

### Decisions / business
- [Decision record](docs/DECISIONS_2026-07-23.md)
- [Buyer validation and paid pilot plan](docs/MARKET_VALIDATION.md)
- [First buyer lead list](docs/BUYER_LEADS_2026-07-23.md)
- [Korean buyer outreach scripts](docs/BUYER_OUTREACH_KR.md)
- [Unit economics hypotheses](docs/UNIT_ECONOMICS.md)
- [30-day execution roadmap](docs/30_DAY_EXECUTION.md)

### Technical
- [Technical MVP and stage gates](docs/TECHNICAL_MVP.md)
- [Current hardware role matrix](docs/HARDWARE_ROLE_MATRIX.md)
- [Radio architecture options](docs/ARCHITECTURE_OPTIONS.md)
- [Korea regulatory gate](docs/REGULATORY_GATE.md)

### Field pilot
- [Operator one-pager](docs/PILOT_ONE_PAGER_KR.md)
- [Objective field-pilot protocol](docs/FIELD_PILOT_PROTOCOL.md)
- [Korean voice test phrases](data/voice_test_phrases_ko.csv)

## Tools

### Validate the v0 packet

```bash
python tools/audio_packet_sim.py --packets 10000
```

Expected v0 application packet:

- header: 16 bytes
- audio: 160 bytes
- total: 176 bytes
- SX1280 LoRa payload ceiling used for the first design check: 255 bytes

The same packet shape is implemented in portable C++ under `firmware/common/pr1_packet.hpp` and is CI-tested with 10,000 round trips plus invalid-input cases.

### Analyze buyer evidence

```bash
python tools/analyze_buyer_interviews.py data/interview_log.csv
```

Reports progress against the current minimum evidence gate:
- 10 interviews
- 5 repeated problems
- 3 customers already spending money/staff effort
- 2 concrete paid-pilot commitments

### Analyze RF/field measurements

```bash
python tools/analyze_rf_log.py data/rf_test_log.csv
```

Summarizes loss, phrase intelligibility and latency by distance/radio mode.

## Data templates

- `data/buyer_leads.csv` — outreach tracker
- `data/interview_log.csv` — buyer interview evidence
- `data/rf_test_log.csv` — engineering field measurements
- `data/pilot_log.csv` — operator + participant field-pilot evidence

## 30-day gate

By **2026-08-22**, PR1 should have:

- 10 buyer interviews,
- a measured live-voice outdoor result,
- at least 2 concrete paid-pilot commitments,
- a credible path to ≥40% hardware gross margin.

Until this gate is passed, avoid mass purchasing, final industrial design, phone-app development, PR2 hardware expansion and large public-market plans.

## Development principle

**One gate at a time. Measure before expanding.**
