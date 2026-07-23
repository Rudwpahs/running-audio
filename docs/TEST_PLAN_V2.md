# PR1 Test Plan v2

Date: 2026-07-23

Rule: **one gate at a time**. A later gate cannot be called successful when an
earlier gate has not passed.

## Evidence standard

Every physical test log must include:

- date/time
- exact TX and RX board revision
- firmware commit SHA
- SX1280 mode and all radio parameters actually set
- power setting
- antenna used
- physical distance / environment
- packets sent / received / missing / duplicate / reordered
- result and next action

Never write `works` when the evidence is only a successful compile or a PC
simulation.

---

## Gate 0 — before hardware arrives

### 0A. RF protocol unit tests

Run:

```bash
python -m unittest tools.test_pr1_rf_protocol -v
```

Pass criteria:

- 10-byte header round-trip
- bad fragment fields rejected
- 640-byte frame -> 6 FLRC-sized packets
- 640-byte frame -> 3 GFSK-sized packets
- 84-byte codec candidate -> one FLRC-sized packet
- reverse-order reassembly is bit exact
- duplicate fragment does not duplicate output

### 0B. Packet budget

Run:

```bash
python tools/rf_packet_budget.py
```

Pass criteria:

- numbers agree with hand calculation
- selected audio format has a documented RF packet cost
- no one claims RF throughput from this calculator alone

---

## Gate 1 — arrival inspection

Do this before flashing custom firmware.

### Checklist

- [ ] photograph front and back of both boards
- [ ] confirm both are the ordered SX1280 2.4G **Without PA** variant
- [ ] record board revision markings
- [ ] record MVSR revision; microphone hardware can differ by revision
- [ ] inspect antenna connector/antenna supplied
- [ ] inspect speaker connector before buying a pigtail
- [ ] check USB data cable actually enumerates the board
- [ ] format microSD FAT32 and verify read/write on PC
- [ ] do not attach the bone-conduction transducer until connector polarity and
      speaker-output wiring are confirmed from the exact board

Failure action: stop and resolve a hardware/version mismatch before code work.

---

## Gate 2 — vendor firmware sanity

Purpose: prove the boards and radio work before blaming PR1 code.

### 2A. Board baseline

- build/flash the official example for the exact board
- confirm serial output on each board
- record the exact repository commit/example path used

### 2B. SX1280 PingPong

Use the vendor SX1280 PingPong example without PR1 protocol changes.

Pass criteria:

- both radios initialize
- A -> B and B -> A packet exchange is observed
- radio settings are copied from logs/source into the test log
- 100 consecutive short packets can be exchanged at bench range under the
  approved/legal test configuration

Do not continue to audio if this fails.

---

## Gate 3 — PR1 deterministic data link

Replace the vendor message body with PR1 v2 packets carrying known byte
patterns. Start with a payload small enough to fit in one packet.

### Phase 3A — single packet

Payload sizes:

- 20 B
- 60 B
- 84 B
- 100 B

For each size, send at least 10,000 packets under a fixed bench condition.

Collect:

- sent
- received
- missing by packet sequence
- duplicates
- reordered packets
- invalid header
- corrupt application payload

Pass target for moving forward:

- zero silent application corruption
- statistics are internally consistent
- any packet loss is visible in logs rather than hidden

### Phase 3B — fragmentation

Known frames:

- 200 B
- 320 B
- 640 B
- 1,000 B

Pass criteria:

- reassembled bytes equal source bytes exactly
- incomplete frames expire and are counted
- a missing fragment never produces a fake complete frame

---

## Gate 4 — audio output without RF

Keep radio out of the problem.

### Source

Use a known WAV/PCM file on microSD or a generated tone.

### Output

Use the onboard MAX98357A and a safe test speaker first. Connect the
bone-conduction transducer only after wiring/output suitability has been
verified.

Pass criteria:

- correct sample rate and channel handling
- repeated playback without buffer underrun
- no clipping at the chosen digital level
- no dependency on the microphone

---

## Gate 5 — coded stored audio over RF

First candidate for link validation:

- 16 kHz
- mono
- 10 ms audio frame
- IMA-ADPCM candidate
- approximately 84 B application frame including small block state

This format is a **test candidate**, not the final music-quality promise.

Pipeline:

```text
microSD PCM
 -> encode block
 -> PR1 header
 -> SX1280
 -> receive
 -> validate sequence
 -> jitter buffer
 -> decode
 -> MAX98357A
```

Measure separately:

- encode time
- RF queue/wait time
- receive/reassembly time
- jitter-buffer depth
- decode time
- I2S/output queue state
- end-to-end latency
- underrun count
- missing frames

Initial acceptance target:

- 10 minutes continuous stored-audio playback without crash
- no unreported frame corruption
- latency and underrun metrics are logged

Audio quality is judged only after technical stability is established.

---

## Gate 6 — codec quality decision

Compare at least:

1. low-complexity ADPCM baseline
2. one higher-quality low-bitrate codec candidate
3. a raw/less-compressed reference when feasible

Use the same source clips:

- speech
- music with vocals
- music with percussion/high frequencies

Record:

- encoded bitrate
- frame size distribution
- CPU load
- RAM use
- encode/decode time
- measured end-to-end latency
- subjective quality notes using the same listening setup

Do not choose the final codec solely from bitrate.

---

## Gate 7 — controlled range testing

Only after the legal/regulatory route for the exact configuration has been
confirmed.

Change one variable at a time:

- distance
- orientation
- radio mode/rate
- allowed power setting
- codec bitrate

Suggested fixed points after venue approval:

- 1 m
- 5 m
- 10 m
- 20 m
- then larger distances only when justified

At each point run enough packets to calculate a meaningful loss rate; do not use
one successful packet as the result.

Stop immediately if the configured operation is outside the confirmed test
conditions or causes harmful interference.

---

## Gate 8 — product transition decision

The development boards have done their job when we can answer:

1. What bitrate is actually needed for acceptable audio?
2. What RF packet rate and packet-loss behavior does that create?
3. What range is achieved under allowed test conditions?
4. What latency is achieved end to end?
5. What battery/current target follows from the measured radio/audio duty cycle?
6. Is the user value still strong enough to justify a custom product?

Only then start:

- production MCU/radio choice
- antenna design
- custom PCB
- battery/charging design
- enclosure
- pre-compliance testing
- KC/conformity path

The LILYGO board is a measurement platform, not a requirement for the final
product.
