# PR1 Field Pilot Protocol

Version: 0.1 — 2026-07-23

## Purpose

A field pilot is **not a demo**.

It must answer four questions with evidence:

1. Can participants understand spoken instructions while moving?
2. Does the system stay reliable at the target distance/environment?
3. Is distribution/collection easier enough to matter to the operator?
4. Will the operator take a concrete next step, including a paid next session?

---

# 0. Do not run a field pilot before these gates

- T1 RF packet link measured
- T2 prerecorded speech stable
- T3 live voice usable, if the pilot requires live voice
- equipment has no exposed/loose power wiring likely to short during movement
- batteries are physically secured
- correct 2.4 GHz antenna is attached before RF transmission
- local radio configuration has been reviewed for the intended test conditions
- operator/site permission received

For a minor developer, use an adult/guardian or responsible site staff member for institutional field tests, equipment responsibility and participant consent logistics.

---

# 1. First pilot environment

Do **not** begin next to vehicle traffic, bicycles, roads, water or other hazards.

First live field pilot should be:
- closed or controlled outdoor area
- flat walking surface
- easy visual contact between operator and participants
- no requirement for the audio system to carry safety-critical commands

PR1 is still a prototype; participants must be able to stop the test and hear normal surroundings without relying on PR1 for safety.

---

# 2. Pilot structure — 60 to 90 minutes

## Phase A — preflight, 10 min

Record:
- date/time
- location
- weather
- participant count
- radio mode/configuration
- TX power setting
- battery start level/voltage if available
- device IDs

Set all receiver volumes low first, then raise to a comfortable speech level.

Start timer when the equipment leaves the case.

### Deployment metric

`setup_time_sec = ready_to_use_time - case_open_time`

Initial target for a small pilot set: **≤180 seconds**.

This is an internal product target, not a customer-facing promise.

---

## Phase B — stationary intelligibility, 10 min

Distance: short controlled range, e.g. 10–20 m.

Use 20 phrases from `data/voice_test_phrases_ko.csv` in random order.

For each phrase, participant records or repeats the key information.

### Measure

`intelligibility = correct_phrases / total_phrases`

Initial internal gate:
- 20 m: **≥95%** phrase accuracy

If this fails, do not move to a longer range pilot. Diagnose audio level, RF loss, buffering and output hardware first.

---

## Phase C — moving group test, 15–20 min

Participants walk/jog at the normal pace of the target program in the controlled area.

Operator gives short real instructions such as:
- stop/start
- direction change
- regroup instruction
- pace change
- numbered instruction

Record:
- missed instructions
- audible dropouts
- operator repeats
- receiver resets/disconnections
- participant stops caused by device trouble

### Core operational metric

`repeat_rate = repeated_instructions / total_instructions`

Compare this with the operator's normal method when possible.

---

## Phase D — distance test, 15 min

Do not turn this into an unstructured “how far can it go?” test.

Use fixed checkpoints:
- 20 m
- 50 m
- 100 m only when T5 engineering tests justify it

At every checkpoint run the same phrase/intelligibility method and log:
- packet loss
- RSSI
- SNR
- latency if available
- phrase accuracy

Initial business-use gate:
- **50 m LOS: ≥90% phrase accuracy** in the intended spoken-instruction scenario

Again, this is an internal MVP gate, not a final range specification.

---

## Phase E — surroundings awareness, 5–10 min

The open-ear value must be measured, not assumed.

Ask participants to rate 1–5:
- speech clarity
- comfort
- awareness of nearby normal conversation/environment
- distraction caused by the device

Do **not** create safety tests involving approaching vehicles, bicycles, alarms or deliberately dangerous situations.

---

## Phase F — collection and operator interview, 10–15 min

Start timer when operator asks for devices back.

Record:
- collection_time_sec
- missing devices
- devices requiring reset
- dirty/damaged devices
- charging/storage friction

Then ask the operator:

1. What problem did this actually solve today?
2. What became worse or harder?
3. Would you use it in your next real session?
4. What would prevent adoption?
5. Who would approve a purchase/rental?
6. What is the next realistic date?
7. Would you schedule the next session at the proposed paid-pilot price?

The last question must include a real amount and date when the prototype is ready for paid use.

---

# 3. Pilot scorecard

## Technical

| Metric | Gate |
|---|---:|
| 20m phrase accuracy | ≥95% |
| 50m phrase accuracy | ≥90% |
| unexplained receiver reboot | 0 |
| setup time for small set | ≤180 sec |
| device loss at collection | 0 |

## User

Do not average everything into one vanity score.

Record separately:
- clarity 1–5
- comfort 1–5
- surroundings awareness 1–5
- dropout noticed yes/no

## Buyer

Strongest signals, in order:
1. gives detailed criticism
2. asks for another test
3. introduces another operator
4. asks for price/quote
5. schedules a paid date
6. pays/repeats

The pilot is a commercial success only when behavior moves up this ladder.

---

# 4. Stop conditions during a pilot

Stop the test immediately if:
- battery becomes abnormally hot/swollen/damaged
- exposed wire or connector creates a short risk
- participant reports pain/discomfort from audio or wearable
- participant needs to enter an unsafe traffic/environment condition
- radio/audio hardware repeatedly crashes and distracts from safe movement
- site operator asks to stop

A failed pilot is useful evidence. Do not keep running only to produce a success video.

---

# 5. Required artifacts after every pilot

Within the same day save:
- `pilot_log.csv` row(s)
- RF log if engineering telemetry was available
- phrase-test results
- operator interview notes
- hardware/software version
- exact next action

Write four lines:
1. What we tested
2. What happened
3. What failed/surprised us
4. Exact next test

---

# 6. No-publicity-by-default rule

Do not publicly upload identifiable participant faces/voices or institutional pilot material merely because it was technically useful.

Separate:
- engineering evidence
- research/feedback records
- marketing/publicity media

Get appropriate permission before using the third category.
