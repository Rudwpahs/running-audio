# PR1 Business Plan v0 — Phone ↔ Receiver Exchange

Date: 2026-08-16

Status: **pre-PoC / market-validation / prototype-definition stage**

This document replaces the older outdoor group-audio framing. PR1 is now defined as a **phone-and-receiver exchange system** for study cafes and parks.

---

# 1. Executive thesis

## Product category

**Phone deposit + audio-only receiver exchange system.**

## Korean definition

> PR1은 스터디카페와 공원에서 사용자가 휴대폰을 맡기고, 대신 수신기를 받아 소리만 듣는 시스템입니다.

## Customer promise

> Users can keep the audio they need while physically separating themselves from the phone screen.

## Core flow

```text
phone in → receiver out → audio only → receiver back → phone back
```

## What PR1 is not

- wireless earbuds
- an AirPods/Buds/Shokz replacement
- an MP3 player
- a generic tour-guide system
- a youth camp audio system
- a vague digital wellness platform
- an app-first service

PR1 only makes sense if the exchange flow is valuable. If the phone stays with the user, PR1 becomes just another audio device and loses its reason to exist.

---

# 2. Beachhead

## Primary hypothesis — study cafes / 독서실

Why:
- users already want to avoid phone distraction
- users may still need audio: music, white noise, lectures, timers, short alerts
- operators can run a controlled counter/station process
- the benefit is easy to explain: phone stays away, audio remains

### Main question

> Will study cafe users actually hand in their phone if they can still hear the audio they need?

## Secondary hypothesis — parks / walking / running areas

Why:
- users may not want to carry or check phones while walking/running/resting
- a park station can create a phone-free audio experience
- the use case is physical and easy to observe

### Main question

> Will park users accept a phone deposit / receiver rental flow for walking or running audio?

## Do not choose from intuition

Survey results, interviews, and small tests decide whether study cafe or park becomes the first pilot.

---

# 3. Problem hypothesis

## Study cafe user problem

A user needs audio but does not want the phone near them:

1. plays music or white noise
2. opens lecture audio
3. uses timer or alarm
4. phone remains on desk/pocket
5. user checks notifications, messages, Shorts, Instagram, games, or browser
6. study session breaks

Existing earbuds do not solve the problem because the phone is still near the user.

## Park user problem

A user wants audio while moving or resting:

1. phone is in hand or pocket
2. user checks screen often
3. phone is heavy or inconvenient during walking/running
4. user still wants sound

Existing earbuds only move the sound, not the phone.

---

# 4. Solution architecture

## MVP experience

- user deposits phone at a PR1 station
- user receives a matched receiver
- receiver plays audio only
- receiver has no screen, no apps, no social feed
- user returns receiver to recover the matched phone

## Station requirements

- phone storage / placement
- transmitter/audio input path
- receiver numbering and matching
- check-in/check-out process
- charging and cleaning process
- basic loss/damage procedure

## Receiver requirements

- small enough to carry to a seat or around a park
- audio-only output
- no tempting screen
- simple power/volume/status
- reliable pairing/matching to the deposited phone or station stream

## Not in MVP

- social app
- recommendation algorithm
- AI features
- public feed
- complicated account system
- final industrial design
- large fleet cloud software

---

# 5. Technical development strategy

The current hardware is a **PoC tool** to answer whether audio can be transmitted from a station/transmitter to a receiver. It is not yet the final product architecture.

## Technical PoC questions

1. Can transmitter → receiver audio run reliably enough for study/park audio?
2. What is the minimum acceptable audio quality?
3. What is the real range needed for a study cafe seat or park loop?
4. How small can the receiver be later?
5. How does the station match phones and receivers safely?

## Stage gates

T0 board bring-up → T1 packet link → T2 prerecorded audio → T3 live/audio-source test → T4 station/receiver exchange simulation → T5 study cafe / park pilot.

Do not skip the exchange-process gate. A working radio link alone does not prove PR1.

---

# 6. Competition

## Existing earbuds

They already solve wireless listening, but they do not remove the phone.

PR1 does not beat earbuds on sound quality. PR1 beats them only if the user wants the phone physically away.

## MP3 players / dedicated players

They remove the phone, but they do not support the exchange/station model tied to the user’s own phone audio and facility flow.

## App blockers / focus modes

They keep the phone near the user and rely on self-control. PR1 changes the physical setup.

## Lockers / phone collection boxes

They remove the phone, but they also remove audio. PR1 keeps audio available.

## Tour-guide systems

They can do one-to-many audio, but PR1 is not primarily about group guiding. The core is phone deposit + personal receiver handoff in study cafe and park scenarios.

### Required differentiation

PR1 must prove:

1. people are willing to deposit the phone
2. audio-only is enough for the use case
3. the receiver feels easier than carrying the phone
4. operators can safely manage phone/receiver exchange
5. the system creates behavior that earbuds cannot

---

# 7. Business model hypotheses

## Model 1 — study cafe pilot

A study cafe runs a small PR1 station with a few receivers.

Potential value:
- stronger phone-free study promise
- premium seating / focus program
- rental/add-on revenue
- differentiation from other study cafes

First pilot should be small and controlled.

## Model 2 — park station pilot

A park/operator/partner runs a phone deposit + receiver rental point for walking/running/resting audio.

Potential value:
- phone-free walking/running experience
- light rental model
- event/program add-on

## Model 3 — facility kit

A starter kit may include:
- station/transmitter unit
- 5–10 receivers
- numbered matching process
- charging/cleaning/storage setup
- simple operating manual

Pricing is not fixed. It must follow pilot evidence.

---

# 8. Market validation plan

## Minimum evidence before building more hardware

Study cafe:
- users say they would hand in the phone
- operator sees a reason to run the process
- at least one controlled pilot location exists

Park:
- users say they would leave the phone at a station
- receiver-only walking/running feels useful
- location/station flow is practical

## Strong evidence

Weak:
- “Good idea”
- “I might use it”

Strong:
- user completes survey and volunteers for test
- user agrees to hand in phone in a controlled test
- operator agrees to host a pilot
- operator discusses price, workflow, risk, and liability

---

# 9. Main risks

## Trust risk

Users may hesitate to hand in phones. This is the biggest risk.

Required work:
- clear matching number
- visible storage method
- privacy explanation
- fast return process
- loss/damage rule

## Audio risk

If audio quality is too low, users will not accept the exchange.

## Operator friction

If staff must spend too much time managing phones and receivers, facilities will reject it.

## Existing habits

Some users may prefer just using self-control, lockers, smartwatches, or MP3 players.

---

# 10. 30-day gate

By the next real gate, PR1 should have:

- survey results separated by study cafe / park use case
- at least 10 user interviews
- at least 3 people willing to try phone deposit + receiver use
- at least 1 operator or location willing to discuss a small pilot
- a basic transmitter → receiver audio demonstration
- an exchange-flow prototype: numbering, matching, return process

## No-go rules

Stop or pivot if:

- users will not hand in phones
- audio-only does not cover the need
- operators will not manage the exchange flow
- the system keeps being explained as earbuds

---

# 11. Development principle

**Do not build an earbud. Build the exchange system.**

The receiver is not the product by itself. The product is the whole flow:

> phone deposit + receiver handoff + audio-only use + safe return.
