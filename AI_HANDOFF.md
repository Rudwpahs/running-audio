# PR1 AI Handoff Log

> Repository: `Rudwpahs/running-audio`
>
> This file is the coordination source for AI work on PR1.
>
> **Critical correction on 2026-08-16:** PR1 is not outdoor group audio and not an earbud-like device. PR1 is a **phone ↔ receiver exchange system**.

---

# 0. Current product definition — source of truth

## Korean one-liner

> PR1은 스터디카페와 공원에서 사용자가 휴대폰을 맡기고, 대신 수신기를 받아 소리만 듣는 시스템입니다.

## Core flow

```text
phone in → receiver out → audio only → receiver back → phone back
```

## What must not be repeated

Do not describe PR1 as:

- wireless earbuds
- screen-free group audio
- outdoor education audio
- youth camp audio
- a Shokz/AirPods replacement
- a generic wireless audio system
- a vague digital wellness platform

## What must be built and validated

PR1 is valid only if the exchange flow works:

1. user hands in phone
2. station/transmitter holds or connects to phone/source
3. user receives matched receiver
4. receiver provides audio only
5. user returns receiver
6. matched phone is returned safely

The receiver alone is not the product. The product is the exchange system.

---

# 1. Roles

## GPT — Lead architect and final reviewer

- Owns product definition, architecture, hardware choices, protocol, acceptance criteria, and final approval.
- Must reject wording that makes PR1 look like ordinary earbuds.
- Produces documentation, implementation drafts, and test plans.

## Claude / other coding agents — Focused implementation reviewer

- Reviews code and documents against the locked product definition above.
- Must not silently broaden PR1 back into generic outdoor group audio.
- Reports conflicts before changing hardware, transport, packet format, audio format, or product flow.

---

# 2. Collaboration rules

1. Read this file before every PR1 task.
2. Treat section 0 as the product definition source of truth.
3. Work on only the user-requested task.
4. Do not silently broaden scope.
5. Do not claim physical hardware tests unless a human supplied results.
6. Build success may be claimed only when a build command was actually run.
7. Use UTC+09:00 dates in `YYYY-MM-DD HH:mm KST` format.
8. Record files changed, tests run, and unresolved risks.
9. When product wording is touched, check it against `docs/PR1_CANONICAL_POSITIONING.md`.

---

# 3. Current state — 2026-08-16

## Product

- Corrected definition: **phone deposit + receiver handoff + audio-only use**
- First use case: **study cafe / 독서실**
- Second use case: **park / walking / running**
- Website and docs are being rewritten around this definition.

## Market validation

- Survey has been distributed.
- Interpretation must focus on whether users accept phone deposit and receiver-only use.
- General interest in wireless audio is not enough.

## Technical

- Existing technical work may still be useful for transmitter → receiver audio.
- Older wording about youth camps/outdoor group audio is deprecated.
- Technical MVP must include an exchange-flow test, not only radio/audio tests.

---

# 4. Current documentation map

Read these first:

- `README.md`
- `docs/PR1_CANONICAL_POSITIONING.md`
- `docs/WEBSITE_COPY_KR.md`
- `docs/BUSINESS_PLAN_V0.md`
- `docs/MARKET_VALIDATION.md`
- `docs/PILOT_ONE_PAGER_KR.md`
- `docs/TECHNICAL_MVP.md`
- `docs/MODOO_STARTUP_PACKET_KR.md`

---

# 5. Current task

## Status

`IN_PROGRESS`

## Goal

Rewrite website copy, business documents, validation documents, and technical docs so every file uses the correct PR1 structure:

> phone deposit + receiver handoff + audio-only use + study cafe / park.

## Acceptance criteria

- No main document should define PR1 as earbuds or generic outdoor group audio.
- Every customer-facing document should start from the phone ↔ receiver exchange.
- Website copy should be plain and direct, not startup buzzwords.
- Technical docs should include station/receiver exchange-flow testing.

---

# 6. Message log

## GPT-20260816-01 GPT -> Future AI workers

- Date: 2026-08-16 16:30 KST
- Reply to: none
- Status: DONE
- Scope: product definition correction

### Goal
Reset PR1's source of truth after the user corrected the product structure.

### Fixed constraints
PR1 is a phone and receiver exchange system. The correct flow is phone in, receiver out, audio only, receiver back, phone back.

### Work completed
Updated the repository docs to begin migrating away from the older outdoor group-audio framing.

### Evidence
User explicitly corrected the structure: PR1 is not a normal receiver/earbud idea; it is a phone and receiver exchange system for study cafes and parks.

### Files changed
- `README.md`
- `AI_HANDOFF.md`
- `docs/PR1_CANONICAL_POSITIONING.md`
- `docs/WEBSITE_COPY_KR.md`
- `docs/BUSINESS_PLAN_V0.md`
- `docs/MARKET_VALIDATION.md`
- `docs/PILOT_ONE_PAGER_KR.md`
- `docs/TECHNICAL_MVP.md`
- `docs/MODOO_STARTUP_PACKET_KR.md`

### Findings / risks
Some older files may still contain outdated youth-camp/outdoor-group-audio wording and need follow-up cleanup.

### Requested next action
Before new implementation, update any stale docs or website source to match the canonical positioning.
