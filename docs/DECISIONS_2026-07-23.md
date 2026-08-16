# PR1 Decisions — Corrected Product Direction

Date: 2026-08-16

This file replaces the older 2026-07-23 direction that framed PR1 as outdoor group audio.

---

# Decision 1 — Product definition

## Decision

PR1 is a **phone ↔ receiver exchange system**.

## Fixed Korean definition

> PR1은 스터디카페와 공원에서 사용자가 휴대폰을 맡기고, 대신 수신기를 받아 소리만 듣는 시스템입니다.

## Reason

If PR1 is described as a wireless receiver or audio device, it looks the same as existing earbuds. The real difference is that the user physically gives up the phone and carries only a receiver.

---

# Decision 2 — Core flow

## Decision

All product, website, business, and technical documents must use this flow:

```text
휴대폰 맡김 → 수신기 받음 → 소리만 듣기 → 수신기 반납 → 휴대폰 찾기
```

English shorthand:

```text
phone in → receiver out → audio only → receiver back → phone back
```

---

# Decision 3 — First markets

## Decision

PR1's first target scenarios are:

1. **스터디카페 / 독서실**
2. **공원 / 산책 / 러닝**

## Not first

- youth camps
- outdoor experiential education
- tourism
- generic group audio
- school-wide broadcast
- general wireless audio market

These may be future pivots only if evidence supports them.

---

# Decision 4 — Competition framing

## Decision

PR1 must not compete as earbuds.

## Explanation

Existing earbuds move sound from the phone to the ear. They do not move the phone away from the user.

PR1's competition is closer to:

- phone lockers
- app blockers
- study cafe phone collection
- self-control systems
- MP3 players
- smartwatches
- existing habits of carrying the phone

---

# Decision 5 — MVP includes exchange process

## Decision

A radio/audio PoC alone is not PR1.

The MVP must include:

- phone slot or mock storage
- receiver ID
- matching process
- check-in/check-out script
- return confirmation
- user trust feedback

## Reason

The hardest problem may not be audio transmission. It may be whether people trust the phone deposit process.

---

# Decision 6 — Website language

## Decision

Website copy must be plain Korean.

Good:

> 폰은 맡기고, 소리만 가져가세요.

Bad:

> 디지털 웰니스 기반 스마트 집중 오디오 플랫폼

## Reason

Startup buzzwords hide the product. PR1 must look real and specific.

---

# Decision 7 — Validation gate

## Decision

Before more hardware expansion, PR1 must prove:

1. users understand the exchange flow
2. users are willing to hand in phones
3. users still need audio during the session
4. operators can manage matching and return
5. the receiver-only experience is better than existing alternatives

---

# Decision 8 — Technical direction

## Decision

Current transmitter/receiver tests remain useful, but they must serve the station-to-receiver system.

Engineering must test:

- station/source → receiver audio
- indoor study cafe distance
- park walking distance
- session matching logs
- receiver return flow

---

# Final rule

Any new document or website section must answer this first:

> Where is the phone, and what does the user carry instead?

If the answer is not “the phone is deposited and the user carries the receiver,” the document is drifting away from PR1.
