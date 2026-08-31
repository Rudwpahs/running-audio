# PR1-DART validation matrix

Use this as the single gate sheet for issues #24–#32.

| ID | Configuration | Host gate | Hardware/lab gate |
|---|---|---|---|
| A | fixed channel, no recovery | packet bounds + baseline simulator | clean-link timing/PER |
| B | hopping only | deterministic/no-repeat AFH test | hopping overhead |
| C | adaptive AFH | channel EWMA/probe test | controlled Wi-Fi coexistence |
| D | AFH + XOR | exact-one-loss FEC test | random/burst loss A/B |
| E | AFH + deadline ARQ | ETA/slack/budget test | useful ARQ ratio |
| F | AFH + XOR + ARQ | integration test | combined airtime/latency |
| G | AFH + PHY | classifier/ladder test | attenuation/NLOS sweep |
| H | full PR1-DART | controller + synthetic matrix | full field matrix |

## Host gates completed by CI

- FLRC packet <=127 B
- deterministic TX/RX channel selection
- no adjacent duplicate hop in long-run test
- uniform channel use with all 40 active
- future map activation and stale version rejection
- channel exclusion/reprobe/reinclusion
- one-erasure XOR recovery, ambiguous recovery rejection
- one-shot deadline ARQ and airtime guard
- stale jitter-frame rejection
- PHY classification/cooldown
- cross-layer state dwell/recovery/airtime guard
- warning-clean C++17 build
- ASan + UBSan pass

## Physical tests still required

- IRQ→SPI p50/p95/p99/max
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

Do not close the hardware-dependent checklist items until measured logs are attached.
