# PR1 30-Day Execution Plan — Exchange System

Date: 2026-08-16

Goal: prove or disprove the corrected PR1 concept:

> 스터디카페와 공원에서 휴대폰을 맡기고 수신기를 받아 소리만 듣는 시스템.

---

# Week 1 — Rewrite and evidence cleanup

## Outcomes

- Website copy uses phone ↔ receiver exchange framing.
- Survey interpretation is reset around study cafe and park.
- Old “outdoor group audio / youth camp” language is removed from core docs.

## Tasks

- Update homepage copy from `docs/WEBSITE_COPY_KR.md`.
- Update all pitch docs to begin with phone deposit + receiver handoff.
- Sort existing survey responses by:
  - study cafe / 독서실 use
  - park / walking / running use
  - willingness to hand in phone
  - audio-only need
  - prototype test interest

## Gate

Can a stranger understand PR1 in 10 seconds without thinking it is earbuds?

---

# Week 2 — User interviews

## Goal

Interview at least 10 people.

## Composition

- 5 study cafe / 독서실 users
- 3 park walkers/runners
- 2 operators or staff candidates

## Must ask

- Would you actually hand in your phone?
- What makes that unsafe or annoying?
- Is audio-only still useful?
- Would you test it for real?

## Gate

At least 3 people agree to a real phone-deposit / receiver-only test.

---

# Week 3 — Technical + exchange mock test

## Technical tasks

- Build basic station/transmitter → receiver audio test.
- Record packet/audio stability.
- Test short indoor distance first.
- Test short park-like movement second.

## Exchange-flow tasks

- Make mock phone slots.
- Make receiver IDs.
- Create check-in/check-out sheet.
- Run a manual session:
  - phone deposited
  - receiver issued
  - audio used
  - receiver returned
  - phone returned

## Gate

A user can complete the whole flow without confusion.

---

# Week 4 — Pilot decision

## Study cafe pilot decision

Proceed if:

- users are willing to deposit phones
- audio-only works for study
- operator workflow looks manageable

## Park pilot decision

Proceed if:

- users accept leaving phone at a station
- receiver range/comfort is acceptable
- return path feels natural

## No-go

Stop or pivot if:

- users reject phone deposit
- audio need is weak
- operators reject phone/receiver management
- product keeps being seen as earbuds

---

# Deliverables by Day 30

- corrected website copy applied
- survey summary by study cafe / park
- 10 interview records
- 3 prototype-test candidates
- station/receiver audio test log
- exchange-flow test log
- next decision: study cafe first, park first, or pivot

---

# Daily rule

Every day, answer one question:

> Did today’s work prove that someone will hand in their phone and use only the receiver?

If not, the work may be technical practice, but it is not product validation.
