# PR1 Field Pilot Protocol — Phone ↔ Receiver Exchange

Date: 2026-08-16

This protocol tests PR1 as a **phone deposit + receiver handoff + audio-only** system.

---

# 1. Pilot purpose

The pilot does not test whether wireless audio is interesting.

It tests whether this flow works:

```text
phone in → receiver out → audio only → receiver back → phone back
```

---

# 2. Pilot types

## Pilot A — Study cafe / 독서실

### Setup

- mock PR1 station near entrance/counter
- phone slot numbers
- receiver numbers
- simple check-in/check-out sheet
- audio source connected to transmitter

### Session

- 30–60 minutes
- user deposits phone
- user receives matched receiver
- user sits away from phone
- user listens to audio only
- user returns receiver
- matched phone is returned

### Measure

- trust in phone deposit
- audio usefulness
- urge to check phone
- confusion in matching/return
- willingness to repeat

## Pilot B — Park / walking / running

### Setup

- fixed mock station at bench/entrance/meeting point
- phone slot numbers
- receiver numbers
- short walking/running path

### Session

- 10–30 minutes
- user deposits phone
- user receives receiver
- user walks/runs/rests with receiver only
- user returns receiver
- matched phone is returned

### Measure

- comfort without phone
- anxiety about phone storage
- range/audio dropouts
- convenience of return path
- willingness to repeat

---

# 3. Safety and trust rules

Before any test:

- explain that this is a prototype test
- explain where the phone will be stored
- assign visible phone slot ID and receiver ID
- do not open the user's phone
- do not view notifications
- do not collect private data from phone screen
- return phone only after receiver return and ID match

If user is uncomfortable handing in phone, do not pressure them. Record the reason.

---

# 4. Check-in sheet

```csv
session_id,date,scenario,user_type,phone_slot_id,receiver_id,start_time,planned_duration,consent,notes
```

# 5. Check-out sheet

```csv
session_id,end_time,receiver_returned,phone_returned,matching_ok,issue,notes
```

# 6. User feedback sheet

```csv
session_id,deposit_trust_1_5,audio_useful_1_5,screen_urge_reduced_1_5,receiver_comfort_1_5,process_easy_1_5,would_repeat,main_reason,main_friction
```

# 7. Technical log

```csv
run_id,session_id,scenario,distance_m,source_type,tx_packets,rx_packets,loss_pct,latency_p50_ms,latency_p95_ms,audio_clear_1_5,dropouts,notes
```

---

# 8. Pass criteria

## Study cafe pass

- user completes session without phone access
- audio remains useful
- deposit trust score ≥ 3/5
- would_repeat = yes or maybe
- main friction is specific and fixable

## Park pass

- user completes route with receiver only
- phone storage anxiety is manageable
- receiver is not too annoying
- audio remains usable
- return process is clear

---

# 9. Fail signals

- user refuses to hand in phone
- user says audio is not needed
- user sees no difference from earbuds
- user is too anxious about phone storage
- operator says workflow is impossible
- receiver adds hassle without reducing phone temptation

---

# 10. Post-pilot interview

Ask after every session:

1. Did you feel okay leaving your phone there?
2. Did you need your phone during the session?
3. Did the receiver give you enough audio?
4. Was this better than just using earbuds?
5. What made it annoying or unsafe?
6. Would you use it again in a study cafe or park?
7. Would you pay for this, or should it be included by the facility?

---

# 11. Pilot conclusion format

After each pilot write:

```md
## Pilot conclusion

- Scenario:
- Did user hand in phone?: yes/no
- Did receiver-only audio work?: yes/no
- Biggest friction:
- Biggest value:
- Repeat interest:
- Operator feasibility:
- Decision: GO / FIX / PIVOT / STOP
```

---

# 12. Core reminder

A good audio test is not enough.

PR1 succeeds only if the exchange behavior works:

> 휴대폰 맡김 → 수신기 받음 → 소리만 듣기 → 수신기 반납 → 휴대폰 찾기
