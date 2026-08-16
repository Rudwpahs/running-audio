# PR1 Unit Economics — Station + Receiver System

Date: 2026-08-16

PR1 is not priced like earbuds. It is a **facility/station system**:

> phone deposit station + transmitter + receivers + charging/storage + operating process.

All numbers here are hypotheses until real prototype cost and pilot evidence exist.

---

# 1. Units to think about

## User session

One person deposits phone, uses receiver, returns receiver, gets phone back.

## Facility station

One study cafe or park point that can manage phones and receivers.

## Receiver fleet

The number of receivers available per station.

---

# 2. Possible business models

## Model A — included facility service

Facility buys or rents PR1 to offer a phone-free audio option.

Possible buyer:
- study cafe owner
- 독서실 owner
- park program operator

## Model B — paid add-on

User pays per use or as a seat/program add-on.

Examples to test:
- 1회 이용료
- 집중석 옵션
- 월 멤버십 포함
- 공원 프로그램 참가비 포함

## Model C — pilot rental

Early stage rental to validate workflow before selling hardware.

---

# 3. Cost structure

## Station costs

- phone holding / slot structure
- transmitter hardware
- audio input path
- power supply
- enclosure
- labels / matching UI
- staff operating material

## Receiver costs

- receiver board/module
- audio output path
- battery
- charging port or dock
- enclosure
- ID label
- strap/clip/case

## Operating costs

- cleaning
- charging
- loss/damage
- staff time
- setup/maintenance
- replacement receivers

---

# 4. First cost sheet template

```csv
item,category,qty,unit_cost,total_cost,notes
station enclosure,station,1,,,
transmitter board,station,1,,,
audio input module,station,1,,,
phone slot materials,station,1,,,
receiver board,receiver,5,,,
audio output module,receiver,5,,,
battery,receiver,5,,,
receiver enclosure,receiver,5,,,
charging accessories,operation,1,,,
labels/cards,operation,1,,,
cleaning supplies,operation,1,,,
spares,operation,1,,,
```

---

# 5. Pricing questions to validate

Do not start with a final price. Ask what pricing structure feels natural.

## Study cafe

- Should this be free as a focus feature?
- Should it be a premium seat option?
- Would users pay per session?
- Would operators pay monthly for the system?
- How much staff time is acceptable?

## Park

- Should it be free for a public program?
- Should it be included in a walking/running event?
- Would users pay a small rental fee?
- How much deposit/trust friction is acceptable?

---

# 6. Early break-even logic

For a facility pilot, calculate:

```text
monthly gross value = paid sessions per month × fee per session
monthly operating cost = staff time + cleaning + replacement reserve
hardware payback months = hardware cost / monthly gross value
```

For a facility sale, calculate:

```text
kit gross margin = kit price - kit COGS
receiver replacement margin = replacement receiver price - receiver COGS
```

Do not claim attractive economics until users and operators accept the exchange flow.

---

# 7. Key economic risk

The biggest cost may not be electronics.

It may be:

- staff time
- trust/safety design
- receiver loss
- phone loss/liability
- cleaning and charging
- low utilization

If the exchange process is too annoying, lower hardware cost will not save the product.

---

# 8. Current gate

Before calculating scale economics, collect:

- actual prototype receiver cost
- station mockup cost
- number of users per day willing to try
- time to check in/out one user
- receiver loss/damage risk
- operator willingness to manage the process

---

# 9. Rule

Do not price PR1 like a pair of earbuds.

Price PR1 as a **system that helps a place run phone-free audio sessions**.
