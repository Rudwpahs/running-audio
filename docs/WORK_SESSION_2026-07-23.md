# PR1 Work Session Handoff — 2026-07-23

## What changed today

PR1 moved from a broad product/business concept into an evidence-gated execution project.

The main repository now contains business decisions, market validation, buyer leads, technical packet code, CI, hardware-role analysis, unit economics, field-pilot protocol and a 30-day GO/PIVOT/PARK gate.

---

# 1. Current product definition

> **Phone-free, one-to-many open-ear group audio for outdoor education, camps and sports coaching.**

Do not describe PR1 primarily as:
- a new earbud
- a long-range music device
- a park-wide audio network

Those frames create the wrong competitors and encourage premature engineering scope.

---

# 2. Current beachhead

## #1 hypothesis
Youth camps / outdoor experiential education operators

## #2 hypothesis
Running / outdoor sports coaches and organizers

This ranking is still a hypothesis.

The next 10 buyer interviews determine the first pilot market.

---

# 3. Evidence already available

## Existing user survey
- 7 current responses
- all are teenagers
- useful for discovering use cases
- **not enough to validate the payer/buyer**

Repeated use cases included outdoor classes/field trips, school PE and running/walking.

## Official market context
2025 year-end Korean youth training facilities: 872 total.

This is context only. PR1's actual relevant percentage is unknown and must not be assumed.

---

# 4. Buyer work prepared

12 first buyer/operator leads are in:
- `docs/BUYER_LEADS_2026-07-23.md`
- `data/buyer_leads.csv`

P0 first calls:
1. 시립목동청소년센터
2. 시립문래청소년센터
3. 시립금천청소년센터
4. 시립보라매청소년센터
5. 시립구로청소년센터
6. 굿러너컴퍼니

Immediate target:
- attempt all 6
- book 3 operator interviews
- complete interviews as soon as operators are available

Do not send a procurement proposal instead of speaking with the actual operator.

---

# 5. Buyer GO gate

Minimum evidence before claiming market validation:

- 10 buyer interviews
- 5+ repeated real problems
- 3+ buyers already spend money or staff effort on the problem
- 2+ concrete paid-pilot commitments with amount + date

Run:

```bash
python tools/analyze_buyer_interviews.py data/interview_log.csv
```

Praise, survey intent and free-test interest do not satisfy the paid-pilot gate.

---

# 6. Technical architecture changed

The old 654-byte-first PCM packet concept should not be the first live voice implementation.

## v0 diagnostic profile
- 8 kHz
- mono
- unsigned PCM 8-bit
- 20 ms
- 160 audio bytes/frame
- 16-byte app header
- **176-byte application packet**

This lets the first voice experiment avoid application-level fragmentation.

The purpose is not sound quality. It is to isolate RF, latency, buffering and dropouts.

---

# 7. Packet code is already prepared

Python:
- `tools/audio_packet_sim.py`

Portable C++:
- `firmware/common/pr1_packet.hpp`

Test:
- `tests/test_packet_codec.cpp`

CI has passed:
- 10,000 Python packet round trips
- 10,000 C++ packet round trips
- invalid magic/version/length/oversize rejection
- Python evidence-tool compilation

The next technical problem is no longer packet serialization.

---

# 8. Hardware roles

Known current arrangement:
- H594 plain T3-S3 SX1280 without PA
- H594-01 T3-S3 MVSR SX1280 without PA

## T1 RF data
Two boards are enough.

## T2 prerecorded voice
Two boards are enough:

`H594 SD/memory -> RF -> H594-01 MVSR audio output`

## T3 live microphone
One audio direction may be missing on the plain H594 endpoint.

Do not solve that by automatically buying a third SX1280 board.

First inventory the delivered order:
- MVSR revision V1.0/V1.1
- external MAX98357A present or not
- external digital mic present or not
- actual bone-conduction/speaker item
- antennas/accessories

Only then purchase the single complementary audio peripheral if required.

Exact previous order-file re-open was attempted during this work session but the File Library retrieval service returned errors, so existing external mic/amp quantity is **not confirmed here**.

---

# 9. Correct technical order

1. T0 official board bring-up
2. T1 1,000 packet RF measurement
3. T2 prerecorded voice
4. inventory / add only missing audio peripheral if needed
5. T3 live microphone
6. 20m / 50m / 100m engineering matrix
7. controlled field pilot

Do not write more feature code when an earlier gate is failing.

---

# 10. Field protocol already prepared

Use:
- `docs/FIELD_PILOT_PROTOCOL.md`
- `docs/PILOT_ONE_PAGER_KR.md`
- `data/voice_test_phrases_ko.csv`
- `data/rf_test_log.csv`
- `data/pilot_log.csv`

Initial internal gates:
- 20m phrase accuracy >=95%
- 50m phrase accuracy >=90%
- unexplained receiver reboot = 0
- setup time <=180 sec for a small pilot set

These are internal development gates, not promised product specifications.

---

# 11. Unit economics hypothesis

Current target model:

- 10-person kit COGS: <=₩720,000
- test ASP: ₩1,390,000
- implied target GM: about 48.2%
- rental price hypothesis: ₩120,000/day
- rental variable-cost allowance: ₩25,000/day
- contribution: ₩95,000/day
- equipment-cost recovery: about 7.6 paid rental days

Business gate:
- hardware GM path >=40%
- rental investment recovery <=12 paid rental uses

These are target economics, not supplier quotes or validated customer prices.

Editable workbook:
`PR1_Execution_Model_2026-07-23.xlsx` was generated in the ChatGPT work session.

---

# 12. Current business plan

Read:
- `docs/BUSINESS_PLAN_V0.md`

It includes:
- market thesis
- target segment
- competition
- technical architecture
- business model
- rental economics
- scalability
- defensibility
- key risks
- capital rule

Do not turn it into a polished investor deck until the evidence gate moves.

---

# 13. Regulatory rule

Read:
- `docs/REGULATORY_GATE.md`

Prototype/research handling and final commercial conformity assessment are different questions.

Before selling a finished wireless product in Korea, confirm the exact final design and applicable conformity-assessment path with RRA/designated testing professionals.

Do not spend final certification money while the radio/antenna/enclosure architecture is still changing.

---

# 14. 30-day project gate

Review date: **2026-08-22**

## GO
- buyer problem supported by evidence
- usable live voice in target scenario
- measured outdoor result
- 2+ paid-pilot commitments
- plausible >=40% hardware gross margin

## PIVOT
Problem is strong but radio/form factor/OEM route should change.

## PARK
Problem is weak, alternatives are sufficient or buyers will not take paid action.

---

# 15. What NOT to do next

Do not:
- restart PR1 from zero
- redesign the whole brand
- build PR2 hardware
- build a phone app
- add AI
- buy many receivers before T3
- claim a final RF range without field data
- treat teenager survey willingness-to-pay as institutional buyer evidence
- create a giant TAM slide to compensate for missing paid pilots

---

# 16. Exact next work session

## Before the boards are available
The highest-value action is **buyer calls**, not more firmware features.

Open:
1. `docs/BUYER_LEADS_2026-07-23.md`
2. `docs/BUYER_OUTREACH_KR.md`
3. GitHub issue #4

Call P0 leads and record results.

## When the boards are physically available
Do not start with live audio.

Open:
1. `docs/HARDWARE_ROLE_MATRIX.md`
2. `docs/TECHNICAL_MVP.md`
3. GitHub issue #5

Inventory the hardware, then begin T0.

---

# One-line state

**PR1 is ready to stop planning and begin collecting buyer + hardware evidence.**
