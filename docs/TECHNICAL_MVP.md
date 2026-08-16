# PR1 Technical MVP — Station to Receiver Audio

Date: 2026-08-16

PR1 is now defined as a **phone ↔ receiver exchange system** for study cafes and parks. The technical MVP must support that product flow.

The goal is not to build earbuds. The goal is to prove that a station/transmitter can keep the phone away while a receiver carries audio only.

---

# 1. Product-level technical goal

## Core flow

```text
phone in → station/transmitter → receiver audio only → receiver back → phone back
```

## Technical MVP question

> Can a PR1 station transmit usable audio to a small receiver while the user's phone stays physically away from the user?

---

# 2. MVP components

## Station / transmitter side

The station is the place where the user leaves the phone.

MVP functions:

- receive or capture audio from a source
- transmit audio to receiver
- represent phone/receiver matching process
- support a clear check-in/check-out flow

The first PoC station may be ugly. It only needs to prove the flow and audio path.

## Receiver side

The receiver is what the user carries.

MVP functions:

- receive station audio
- output sound
- no distracting screen
- simple status/power/volume behavior
- numbered or matched identity for return process

## Matching process

This is part of the product, not paperwork.

MVP must track:

- phone id / slot id
- receiver id
- user session start/end
- return confirmation

A simple paper label or spreadsheet is acceptable for the first human-run test. Later this may become a physical station UI.

---

# 3. Use-case technical requirements

## Study cafe

Primary constraints:

- short to medium indoor distance
- stable audio at a desk
- quiet operation
- low staff friction
- clear numbering/matching
- no phone screen at seat

Audio types:

- music test audio
- lecture/speech audio
- white noise
- timer/alert tone

## Park

Primary constraints:

- user moves away from station
- body blocking and distance may matter
- receiver must feel light
- return path must be clear
- station must be trusted enough for phone deposit

Audio types:

- music test audio
- walking/running timer
- spoken cue
- alert tone

---

# 4. Technical PoC stages

## T0 — Board bring-up

### Do

- USB serial works
- board revision is recorded
- audio output works
- basic data link works
- log files or serial logs are captured

### PASS

Each independent hardware function runs for at least 10 minutes without crash.

---

## T1 — Data link 1→1

### Do

- transmitter sends numbered packets
- receiver logs packet count and sequence gaps
- start with short indoor distance

### PASS

Receiver logs packets and sequence values reliably enough to move to audio test.

---

## T2 — Prerecorded audio 1→1

### Do

- station sends prerecorded speech/music/white-noise sample
- receiver outputs sound
- record drops, delay, and subjective clarity

### PASS

User can recognize the audio and receiver runs continuously for a short session.

---

## T3 — Live or phone-source audio simulation

### Do

- simulate phone audio source at station
- transmit to receiver
- test study cafe desk distance and park short-walk distance

### PASS

Audio is usable enough for the intended behavior test.

---

## T4 — Exchange-flow simulation

This is mandatory. Do not treat radio success as PR1 success.

### Do

- label a phone slot
- label a receiver
- run check-in/check-out
- user gives phone, receives receiver
- user uses receiver only
- user returns receiver and gets phone back

### PASS

User understands the flow and does not feel the process is confusing or unsafe.

---

## T5 — Study cafe / park pilot

### Study cafe pilot

- user leaves phone at mock station
- sits away from phone
- uses receiver for 30–60 minutes
- records comfort, distraction, audio usefulness, and trust

### Park pilot

- user leaves phone at mock station
- walks/runs short route with receiver
- records range, convenience, anxiety, and return flow

### PASS

At least a few users say the exchange flow is worth testing again, and the main problems are specific enough to fix.

---

# 5. Audio profile for PoC

Start simple. Do not over-optimize music quality before the exchange flow is validated.

## Initial diagnostic profile

- speech-first or low-quality audio acceptable
- mono acceptable
- packet loss and latency must be logged
- receiver crash/reboot must be unacceptable

The exact RF/audio profile may change. The first goal is not final sound quality. The first goal is to prove station-to-receiver audio and the phone-away behavior.

---

# 6. Packet and link notes

Older docs may describe outdoor one-to-many group audio. That is no longer the product definition.

However, the engineering lessons still matter:

- sequence numbers for packet loss
- timestamp/latency logging
- small payloads before fragmentation
- jitter buffer
- receiver stability

Keep these technical practices, but aim them at the station/receiver exchange system.

---

# 7. Measurement logs

## Audio link CSV

```csv
run_id,date,scenario,location,distance_m,source_type,packet_mode,tx_packets,rx_packets,loss_pct,latency_p50_ms,latency_p95_ms,audio_clear_1_5,notes
```

## Exchange-flow CSV

```csv
session_id,date,scenario,user_type,phone_slot_id,receiver_id,checkin_time,checkout_time,deposit_trust_1_5,audio_useful_1_5,screen_urge_reduced_1_5,main_friction,would_repeat,notes
```

---

# 8. Technical kill / pivot rules

- If users will not hand in phones, do not keep improving receiver audio as if the product is validated.
- If check-in/check-out is confusing, fix process before adding features.
- If audio works but the product looks like earbuds, rewrite UX and physical station design.
- If station/receiver matching feels unsafe, the product cannot move forward until trust design is fixed.
- If study cafe range is easy but park range is hard, split the roadmap instead of forcing one architecture.

---

# 9. What not to build yet

Do not build yet:

- final app
- final enclosure
- large receiver fleet
- cloud account system
- complex recommendation/audio service
- wide-area park network
- consumer earbud-style branding

Build first:

- working station/transmitter
- receiver audio output
- phone-slot / receiver-id matching
- check-in/check-out script
- basic pilot logs

---

# 10. Development principle

**A working receiver is not PR1. A working phone ↔ receiver exchange is PR1.**
