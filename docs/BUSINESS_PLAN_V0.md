# PR1 Business Plan v0 — Evidence Before Scale

Date: 2026-07-23

Status: **pre-PoC / pre-revenue / buyer-validation stage**

This is not a fundraising narrative. It is the current operating plan and must change when measured evidence contradicts it.

---

# 1. Executive thesis

## Product category

**Phone-free, one-to-many open-ear group audio system for outdoor education, camps and sports coaching.**

## Customer promise

> A facilitator can speak to a moving group without making every participant use a smartphone, while participants remain aware of normal surroundings.

## What PR1 is not

- consumer Hi-Fi earbuds
- a Shokz replacement
- a park-wide public broadcast network
- a generic tour-guide radio
- an app-first service

The project wins only if it makes **group operation** meaningfully better than existing speakers, tour-guide systems, phones and ordinary open-ear headphones.

---

# 2. Beachhead

## Primary hypothesis

**Youth camps / outdoor experiential education operators**

Why:
- recurring group instruction
- participants may move and spread out
- phone-free operation can matter
- surrounding awareness can matter
- real programs exist now and can be observed/interviewed

## Secondary hypothesis

**Running / outdoor sports coaches and program operators**

Why:
- highly mobile group
- short coaching instructions
- repeated sessions
- private operators may make pilot/purchase decisions faster than public institutions

## Do not choose between them from intuition

The first 10 buyer interviews determine which segment receives the first real pilot.

---

# 3. Market evidence available today

The Korean Ministry responsible for youth policy reports **872 youth training facilities as of 2025-12-31**:

- 691 public
- 181 private
- Seoul: 77 total (72 public / 5 private)

Official sources:
- https://www.mogef.go.kr/mp/pcd/mp_pcd_s001d.do?bbtSn=704872&mid=plc502
- https://www.mogef.go.kr/sp/yth/sp_yth_f015.do

This does **not** mean all 872 are PR1 customers. The share with the target problem, budget and willingness to change equipment is unknown.

Current Seoul evidence is stronger on **use case existence** than willingness to pay: the 2026 summer youth companion camp uses 24 municipal youth facilities, about 350 programs and 420 youth, including experiential/environmental activities.

Source:
- https://www.seoul.go.kr/news/news_report.do?bbsNo=158&nttNo=460647

### Bottom-up internal scenario

Using the current internal pricing hypothesis:
- 10-person kit ASP: ₩1,390,000
- 2 kits/customer

| Scenario | Customers | Hardware revenue |
|---|---:|---:|
| 5% of 872 facilities | 44 | about ₩122.3M |
| 10% | 87 | about ₩241.9M |
| 20% | 174 | about ₩483.7M |

These are **scenario calculations, not TAM claims or forecasts**.

The real company opportunity can be larger only if sports, tourism, events or international markets validate the same reusable platform. Do not count those markets before evidence exists.

---

# 4. Problem hypothesis

A target operator experiences one or more of the following repeatedly:

1. facilitator instructions are not heard consistently across a moving outdoor group
2. facilitator repeats/shouts instructions, increasing effort
3. loudspeaker use is awkward, noisy or uneven
4. phone-based delivery introduces screens, notifications, pairing or user-support friction
5. closed headphones are undesirable where surrounding awareness matters
6. existing tour-guide equipment creates distribution/charging/hygiene/fit/collection friction

The interview must discover which of these actually costs money or staff effort.

---

# 5. Solution architecture

## MVP

- 1 transmitter
- small set of receivers
- speech-first audio
- participant phone not required
- open-ear / bone-conduction output experiment
- sequence/loss/latency telemetry
- simple numbered-device operating process

## Not in MVP

- phone app
- GPS
- AI
- accounts
- park repeaters
- Hi-Fi music
- final injection-molded enclosure
- complex fleet cloud software

---

# 6. Technical development strategy

## Current PoC platform

- T3-S3 H594: SX1280 2.4 GHz, without PA
- T3-S3 MVSR H594-01: SX1280 2.4 GHz, without PA + audio expansion

The current hardware is a **learning/PoC vehicle**, not the guaranteed final product radio architecture.

## v0 audio transport

- 8 kHz
- mono
- 8-bit PCM
- 20 ms frames
- 160 audio bytes/frame
- 16-byte application header
- 176-byte application packet

Reason: prove spoken voice transport without starting with fragmentation/reassembly or a codec.

## Stage gates

T0 board bring-up → T1 packet RF → T2 prerecorded voice → T3 live voice → T5 outdoor matrix.

Do not skip a broken stage by adding more software.

## Final architecture review later

Compare using measured evidence:
- proprietary SX1280
- Bluetooth LE Audio / Auracast
- OEM group-audio platform

The product moat must not depend only on owning a custom RF protocol.

---

# 7. Competition

## Existing group-audio / tour-guide equipment

Already offers:
- one-to-many transmission
- mature receivers
- long battery life
- meaningful outdoor ranges
- charging/storage options

Therefore “one transmitter talks to many receivers” is **not** differentiation.

## Open-ear headphone brands

Already win on:
- industrial design
- sound quality
- comfort
- battery
- consumer brand

PR1 should not fight them on consumer headphone quality.

## Auracast

Standardized broadcast audio can reduce the long-term uniqueness of proprietary one-to-many radio.

### Required differentiation

PR1 must prove a bundle of:
1. participant phone-free
2. open-ear outdoor operation
3. fast distribution/collection
4. easy fleet charging/storage
5. operator-first controls
6. lower total operating friction for the chosen niche

---

# 8. Business model

## Model 1 — paid pilot / rental first

Why start here:
- lowers buyer risk
- provides repeated real-world tests
- exposes operational costs
- creates behavioral willingness-to-pay evidence before manufacturing

Current experiment:
- small 60–90 minute paid pilot: ₩50,000
- longer/repeat pilot: ₩100,000–₩120,000
- 1-day mature rental hypothesis: ₩120,000

Prices are experiments, not final price list.

## Model 2 — 10-person starter kit

Current test ASP: **₩1,390,000**

Target COGS: **≤₩720,000**

Target gross margin: **≥40%**

At the current target-cost model:
- gross profit ≈ ₩670,000
- gross margin ≈ 48.2%

## Model 3 — expansion / replacement

Potential:
- added receivers
- replacement units
- batteries/service
- charging/storage case
- annual maintenance

## Model 4 — software only after pain appears

Possible future fleet features:
- inventory/status
- battery monitoring
- channel configuration
- content management

Do not build subscription software before buyers demonstrate a recurring management problem.

---

# 9. Rental economics

Current internal hypothesis:
- rental revenue/day: ₩120,000
- variable handling/delivery/cleaning allowance: ₩25,000
- contribution: ₩95,000
- target kit COGS: ₩720,000

Equipment-cost payback ≈ **7.6 paid rental days**.

Internal gate: ≤12 paid rentals.

Real field data must add:
- labor
- delivery
- damage
- loss
- battery degradation
- payment fees
- storage

---

# 10. Go-to-market

## Phase 1 — interviews

12 current leads prepared; first 6 are P0.

Goal:
- 10 interviews
- 5 repeated real problems
- 3 customers currently spending money/staff effort
- 2 concrete paid-pilot commitments

## Phase 2 — pilot

First segment only.

Use objective protocol:
- 20m/50m phrase accuracy
- dropouts
- setup time
- instruction repeat rate
- operator next action

## Phase 3 — rental

Before mass production, operate several small kits repeatedly.

Learn:
- failure rate
- cleaning time
- setup time
- loss rate
- charging workflow
- actual gross contribution

## Phase 4 — sale

Only after:
- repeat buyer demand
- stable hardware architecture
- compliance path
- reliable BOM

## Phase 5 — public/large institutional market

Approach public institutions with field evidence, a compliant product and operating data rather than an idea deck.

---

# 11. Scalability design

Scale by standardizing only three core hardware families:

1. transmitter/controller
2. receiver
3. charge/storage system

Avoid customer-specific electronics.

Customize through:
- labels/accessories
- configuration
- support
- optional software/content

The company becomes more scalable when manufacturing can be outsourced while PR1 retains:
- product specification
- firmware
- operational workflow
- customer relationships
- field data
- brand

---

# 12. Defensibility

Weak moat:
- SX1280 itself
- open-ear form factor alone
- generic one-to-many transmission

Potential stronger moat:
- field-tested low-friction operating workflow
- fleet charging/collection design
- voice transport/robustness tuning
- vertical-specific accessories
- institutional/customer relationships
- actual outdoor failure dataset
- future management software where justified
- design/IP around unique physical operation, if genuinely novel

Patents should protect a validated product advantage, not substitute for one.

---

# 13. Key risks and explicit response

## Buyer risk
Nobody pays because existing speakers/tour systems are good enough.

**Response:** 10 buyer interviews + paid-pilot ask before further scope.

## Technical risk
Voice link is unstable or inefficient.

**Response:** T1/T2/T3 gates, fixed logs, modulation benchmark.

## Architecture risk
Auracast or OEM is simply better than proprietary SX1280.

**Response:** treat radio technology as replaceable until architecture review.

## Hardware operating risk
Charging, breakage, hygiene and collection destroy economics.

**Response:** rental pilots before sales; measure handling time/loss.

## Compliance risk
Prototype architecture cannot economically become a compliant product.

**Response:** early RRA/lab consultation, but delay expensive final testing until design stabilizes.

## Founder execution risk
Too many parallel tasks or restarting from zero.

**Response:** one gate at a time; no PR2 hardware, app, final branding or large public-market work before 2026-08-22 gate.

---

# 14. 30-day decision

Review date: **2026-08-22**

## GO
- buyer gate met or clearly trending with real paid behavior
- T3 live voice usable for target scenario
- 50m field evidence acceptable
- path to ≥40% hardware gross margin plausible

## PIVOT
Problem is real but solution architecture/form factor must change.

Examples:
- use Auracast
- use OEM radio
- change receiver form factor
- narrow use case

## PARK
- problem weak
- buyer satisfied with alternatives
- no paid behavior after sufficient interviews/pilot exposure

Parking the project is better than manufacturing an unvalidated product.

---

# 15. Next capital decision

Do **not** calculate a large seed round yet.

Next capital should be only what is needed to cross the next gate:
- missing complementary audio peripheral if inventory confirms it is needed
- small number of additional receivers only after T3
- basic cases/charging for a real pilot
- required compliance consultation

Capital follows evidence; evidence does not follow capital.
