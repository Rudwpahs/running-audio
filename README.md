# PR1 — Phone ↔ Receiver Exchange Audio System

PR1 is not an earbud, not a generic outdoor speaker system, and not a normal Bluetooth audio product.

PR1 is a **phone-and-receiver exchange system** for study cafes and parks:

> The user leaves their phone at a PR1 transmitter/storage station, receives a small PR1 receiver, and hears only audio while the phone screen stays away.

The core product is the **exchange flow**:

```text
phone in → receiver out → audio only → receiver back → phone back
```

## One-line Korean definition

> PR1은 스터디카페와 공원에서 사용자가 휴대폰을 맡기고, 대신 수신기를 받아 소리만 듣는 시스템입니다.

## What problem does PR1 solve?

### Study cafe / 독서실

Students often need audio for music, white noise, lectures, timers, or alerts. Existing earbuds still keep the phone on the desk, in the pocket, or within reach. That means the screen, messages, Shorts, Instagram, and other apps remain one touch away.

PR1 removes the phone from the seat while keeping audio available.

### Park / walking / running

People want music, timers, or workout audio in a park without holding a phone, checking the screen, or carrying it during walking/running. PR1 keeps the phone at a fixed station and lets the user move with only the receiver.

## What PR1 is

- A phone deposit / receiver rental flow
- A transmitter/storage station that keeps the user's phone physically away
- A small receiver that receives only audio
- A study cafe and park use-case first
- A system that makes screen access intentionally inconvenient

## What PR1 is not

- wireless earbuds
- an AirPods/Buds/Shokz replacement
- an MP3 player
- a generic tour-guide radio
- a park-wide Hi-Fi broadcast network
- an app-first service
- a vague “digital wellness platform”

If PR1 is explained as “a small wireless audio device,” it sounds like existing earbuds. The correct explanation always starts with **phone ↔ receiver exchange**.

## Current status — 2026-08-16

- Product definition corrected: **phone deposit + receiver handoff + audio-only use**
- Beachhead 1: **study cafes / 독서실**
- Beachhead 2: **parks / walking / running areas**
- Technical PoC: audio transmitter → receiver pipeline
- Early hardware: ESP32-S3 / SX1280 / audio modules are PoC tools, not final product decisions
- Market validation: survey and interviews must measure whether people will actually hand in their phone and use the receiver

## Core operating scenarios

### Scenario A — Study cafe

1. User enters the study cafe.
2. User leaves phone at the PR1 station.
3. User receives a numbered PR1 receiver.
4. User studies with audio only.
5. User returns the receiver.
6. Staff/system returns the matched phone.

### Scenario B — Park

1. User arrives at a park PR1 station.
2. User leaves phone at the station.
3. User receives a PR1 receiver.
4. User walks, runs, or rests while hearing audio only.
5. User returns the receiver and gets the phone back.

## Start here

### Product / business
- [Canonical positioning](docs/PR1_CANONICAL_POSITIONING.md)
- [Website copy](docs/WEBSITE_COPY_KR.md)
- [Business plan](docs/BUSINESS_PLAN_V0.md)
- [Market validation](docs/MARKET_VALIDATION.md)
- [Pilot one-pager](docs/PILOT_ONE_PAGER_KR.md)
- [Modoo Startup packet](docs/MODOO_STARTUP_PACKET_KR.md)

### Technical
- [Technical MVP and stage gates](docs/TECHNICAL_MVP.md)
- [Current hardware role matrix](docs/HARDWARE_ROLE_MATRIX.md)
- [Radio architecture options](docs/ARCHITECTURE_OPTIONS.md)
- [Korea regulatory gate](docs/REGULATORY_GATE.md)

## Development principle

**Do not build an earbud. Build the exchange system.**

Measure one question at a time:

1. Will users actually hand in their phone?
2. Is audio-only enough for the study cafe / park use case?
3. Can staff or a station safely match each phone with the correct receiver?
4. Is the receiver small, reliable, and cheap enough for repeated operation?

Until these are proven, avoid final industrial design, app development, large purchasing, and broad market claims.
