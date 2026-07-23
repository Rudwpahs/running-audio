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
- v0 audio profile: **8 kHz / 8-bit / mono / 20 ms (160-byte audio frame)**
- Final radio architecture: **not decided** — SX1280, Bluetooth LE Audio/Auracast and OEM routes will be compared after field and buyer evidence.

## Start here

- [Decision record](docs/DECISIONS_2026-07-23.md)
- [Technical MVP and stage gates](docs/TECHNICAL_MVP.md)
- [SX1280 implementation and M0 runbook](docs/SX1280_IMPLEMENTATION.md)
- [Buyer validation and paid pilot plan](docs/MARKET_VALIDATION.md)
- [Unit economics hypotheses](docs/UNIT_ECONOMICS.md)
- [30-day execution roadmap](docs/30_DAY_EXECUTION.md)
- [Radio architecture options](docs/ARCHITECTURE_OPTIONS.md)
- [Korea regulatory gate](docs/REGULATORY_GATE.md)

## Tools

Validate the original v0 application packet proposal:

```bash
python tools/audio_packet_sim.py --packets 10000
```

The initial `TECHNICAL_MVP.md` application-packet proposal is:

- header: 16 bytes
- audio: 160 bytes
- total: 176 bytes
- SX1280 LoRa payload ceiling used for that first design check: 255 bytes

The M0 deterministic RF implementation separately uses a **10-byte compact
transport header + 90-byte test pattern = 100 bytes total** so packet loss,
corruption and mode constraints can be measured before audio. The final audio
header is intentionally not frozen until the T1 data-link measurements are
available.

Run all local protocol checks:

```bash
bash scripts/ci.sh
```

Controlled M0 logs can be compared with:

```bash
python tools/analyze_m0_run.py tx.log rx.log
```

## 30-day gate

By **2026-08-22**, PR1 should have:

- 10 buyer interviews,
- a measured live-voice outdoor result,
- at least 2 concrete paid-pilot commitments,
- a credible path to ≥40% hardware gross margin.

Until this gate is passed, avoid mass purchasing, final industrial design, phone-app development, PR2 hardware expansion and large public-market plans.

## Development principle

**One gate at a time. Measure before expanding.**
