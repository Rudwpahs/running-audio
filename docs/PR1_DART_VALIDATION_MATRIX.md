# PR1-DART validation matrix

Use this as the single gate sheet for issues #24–#32 plus the #40/#41 pre-activation safety hardening.

| ID | Configuration | Host gate | Hardware/lab gate |
|---|---|---|---|
| A | fixed channel, no recovery | packet bounds + baseline regression | clean-link timing/PER |
| B | hopping only | deterministic/no-repeat AFH test | hopping overhead |
| C | adaptive AFH | channel EWMA/probe test | controlled Wi-Fi coexistence |
| D | AFH + XOR | exact-one-loss + logical-group freshness | random/burst loss A/B |
| E | AFH + deadline ARQ | ETA/slack/budget + logical one-shot/commit | useful ARQ ratio |
| F | AFH + XOR + ARQ | integration test | combined airtime/latency |
| G | AFH + PHY | classifier/ladder test | attenuation/NLOS sweep |
| H | full PR1-DART | controller safety + structural matrix | full field matrix |

## Host gates completed by CI

### Packet / memory safety
- FLRC application packet <=127 B
- PR1-DART header remains 16 B
- PR1-DART baseline packet remains 116 B
- warning-clean C++17 build
- ASan + UBSan pass

### Long-session identity and time
- wire `uint16_t` sequence is unwrapped to a monotonic `uint64_t` logical frame identity
- exact half-range (`0x8000`) ambiguity is rejected rather than guessed
- first and multiple 16-bit sequence wraps remain monotonic
- jitter deadline regression at logical frame +32768 is covered
- >15 minute and >1 hour logical soak cases are covered without real-time waiting
- delayed pre-wrap frames remain old after wrap
- prior-session generation frames are rejected
- common-layer jitter/ARQ absolute deadlines use 64-bit microseconds

### AFH / FEC / ARQ
- deterministic TX/RX AFH channel selection
- no adjacent duplicate hop in long-run test
- uniform channel use with all 40 active
- AFH pending activation uses the shared logical frame timeline, including beyond `uint32_t`
- channel exclusion/reprobe/reinclusion primitives remain covered
- one-erasure XOR recovery and ambiguous recovery rejection
- stale parity cannot become current merely because a low 16-bit FEC group ID wrapped
- deadline ARQ keeps the compact feedback format
- ARQ one-shot identity uses the full logical frame, not raw `uint16_t`
- queue/TX failure releases an ARQ reservation
- ARQ `sent` increments only after explicit successful TX commit
- committed repair cannot repeat
- stale jitter-frame rejection remains covered

### Controller / telemetry safety
- adaptive controller features default OFF
- `PROCESSING_LIMITED` has priority over RF burst/weak-link actions when observed RX-processing metrics exceed configured thresholds
- processing-limited actions shed optional probe/FEC/ARQ work before adding radio/CPU load
- processing thresholds default to uncalibrated/disabled; hardware data must set them
- configured GOOD PER threshold participates in recovery hysteresis
- adaptive jitter target changes only when explicitly enabled
- unobserved RF counters are omitted from telemetry
- observed zero remains distinguishable from unobserved

## Structural simulator boundary

`tools/pr1_dart_sim.py` is a deterministic **structural regression harness**, not an RF propagation, airtime, queueing, CPU-load, or field-performance model. Its A–H matrix and blind-vs-deadline ARQ comparison may catch logic regressions under identical synthetic inputs, but they must not be cited as evidence that adaptive AFH/FEC/ARQ/PHY improves a real SX1280 link.

Physical or performance claims require measured logs from the hardware/lab gates below.

## Physical tests still required

- exact physical T3-S3-MVSRBoard revision/radio verification
- fixed-channel SX1280 runtime before adaptive layers
- IRQ→SPI p50/p95/p99/max
- SPI/RX-processing/RX-rearm p50/p95/p99/max
- radio queue depth and scheduler misses under load
- reproduce the 500/300/250/225/200/175/150/125 µs TX-gap sweep
- distinguish receiver processing saturation from RF weakness using timing + queue + RSSI/PER evidence
- actual Opus complexity benchmark on ESP32-S3
- clean LOS raw PER target <0.1%
- scheduler misses = 0
- controlled interferer final loss target <1% (initial target)
- temporary interferer recovery target <2 s (initial target)
- 5 m body block
- 10 m worn receiver
- 20 m worn receiver
- front/back/side orientation
- stationary/walking/turning
- wall/door/corner NLOS
- crowded environment
- RF airtime/duty and power consumption

## Activation rule

Software-common code being present or CI-green does not mean it is safe to enable on hardware. Activation order remains:

```text
fixed RF + instrumentation
→ reproduce/classify RX timing bottleneck
→ static hopping
→ adaptive map
→ XOR FEC
→ deadline ARQ
→ PHY ladder
→ cross-layer controller last
```

Do not close hardware-dependent checklist items until measured logs are attached. Do not call the integrated audio stack validated until the long-session software gates **and** the relevant hardware A/B gates are both satisfied.
