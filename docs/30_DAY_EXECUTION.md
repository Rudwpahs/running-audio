# PR1 30-Day Execution Roadmap

Period: **2026-07-23 → 2026-08-22**

## North Star for this month

By 2026-08-22, prove all three:

1. **Buyer problem** — a real operator has a repeated problem and pays/acts to solve it.
2. **Technical proof** — live voice works through the PR1 radio/audio path in a real outdoor test.
3. **Paid intent** — at least two buyers agree to a concrete paid pilot amount and date.

Do not expand PR2, public-site polish, mass purchasing, or final industrial design before this gate.

---

# Week 1 — Buyer evidence + technical preparation
## 2026-07-23 ~ 2026-07-29

### Business
- [ ] Build a list of 20 buyer candidates.
- [ ] Contact at least 10.
- [ ] Complete at least 5 buyer interviews.
- [ ] Record current equipment, price, group size, usage frequency, approval process.
- [ ] Ask for one introduction at the end of every useful interview.

### Technical
- [ ] Confirm exact board variants/revisions when hardware is available.
- [ ] Prepare official LILYGO examples and RadioLib toolchain.
- [ ] Create packet/log schema before writing audio transport.
- [ ] Prepare 8 kHz/8-bit/mono speech test file.

### Week 1 exit gate
- 5 buyer interviews complete.
- At least 2 show a repeated problem.
- T0 bring-up checklist is ready or completed if hardware has arrived.

---

# Week 2 — RF → prerecorded voice → live voice
## 2026-07-30 ~ 2026-08-05

### Technical order — do not skip
1. T0 board bring-up
2. T1 1,000 packet RF test
3. T2 prerecorded speech
4. T3 live microphone

### Deliverables
- [ ] CSV RF logs
- [ ] modulation settings recorded
- [ ] packet loss calculated
- [ ] audio recording/video of successful test
- [ ] known-failures list

### Business
- [ ] Finish all 10 buyer interviews.
- [ ] Rank segments again using actual evidence.
- [ ] Choose only one pilot segment.

### Week 2 exit gate
- Live voice 1→1 works for 10 minutes.
- Median latency target ≤250ms or the actual measured result and bottleneck are known.
- 10 buyer interviews complete.

---

# Week 3 — Outdoor validation + pilot design
## 2026-08-06 ~ 2026-08-12

### Technical
- [ ] 20m LOS test
- [ ] 50m LOS test
- [ ] 100m LOS test
- [ ] body-block / partial NLOS test
- [ ] compare at least two SX1280 modulation configurations
- [ ] open-ear/bone-conduction output comfort test

### UX operations
Time the full process:
- devices out of case
- power on
- channel ready
- distribute
- collect
- charge/store

Target setup time: ≤3 minutes for a small pilot set.

### Pilot package
Prepare:
- one-page concept
- 60-minute test script
- safety/operation checklist
- feedback form
- paid-pilot quote hypothesis

### Week 3 exit gate
- 50m result is measured, not guessed.
- one buyer gives a real pilot date.

---

# Week 4 — Field pilots + decision
## 2026-08-13 ~ 2026-08-22

### Field
- [ ] Pilot #1
- [ ] Pilot #2
- [ ] Pilot #3 if possible

For each pilot record:
- participant count
- environment
- setup time
- voice intelligibility
- dropout incidents
- battery use
- participant comfort
- surroundings awareness
- operator problem solved/not solved
- operator's next action

### Commercial ask
At the end of the pilot, do not ask only "어떠셨어요?"

Ask:
> 다음 회차를 같은 규모로 유료 운영한다면 ₩50,000 / ₩100,000 수준에서 실제 일정을 잡으실 수 있습니까?

Record exact response.

### Final decision — 2026-08-22

## GO
- Technical T3/T5 is usable for the target scenario.
- 2+ paid pilot commitments.
- target unit economics can plausibly reach ≥40% GM.

## PIVOT
Problem is strong but the solution needs different radio, form factor, or OEM architecture.

## STOP / PARK
No real buyer problem or no paid behavior after 10+ buyer interviews and pilot exposure.

---

# Daily operating rule

At the beginning of each work session write:

> 오늘 끝났다고 말하려면 어떤 **파일 / 수치 / 증거**가 남아야 하는가?

At the end write only four lines:
1. What I tested
2. What happened
3. What failed / surprised me
4. Exact next test

---

# Do-Not-Do List for these 30 days

- [ ] redesign the whole brand
- [ ] restart the project from zero
- [ ] buy many additional receivers before T3
- [ ] develop a phone app
- [ ] develop PR2 hardware
- [ ] build AI features
- [ ] claim a final range before outdoor measurement
- [ ] use teen survey responses as buyer willingness-to-pay evidence
- [ ] spend time estimating giant global TAM before first paid pilot

The rule is: **one gate at a time.**
