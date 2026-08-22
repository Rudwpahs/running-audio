# PR1 RF Checkpoint — 2026-08-23 02:32 KST

This checkpoint preserves the exact working state before the next reliability-layer implementation.

## Current verified hardware / transport
- Boards: 2 × LILYGO T3-S3 MVSRBoard V1.1 (ESP32-S3 + SX1280)
- A transmitter: Galaxy USB Audio -> ESP32-S3 -> IMA-ADPCM -> SX1280 FLRC
- B receiver: SX1280 FLRC -> IMA-ADPCM decode -> MAX98357A speaker
- RF frequency: **2476.0 MHz**
- FLRC bitrate: **650 kbps**
- FLRC coding-rate argument: **2 = CR 1/2**
- TX power: **3 dBm**
- Preamble: **16 bits**
- RF payload: **100 bytes**
- Post-TX gap: **225 us**
- Audio: **32 kHz mono IMA-ADPCM**
- Samples per RF packet: **165**
- Audio block duration: **5.15625 ms**
- Continuous/unlimited phone playback enabled

## Changes actually applied on hardware
### A transmitter
- Instrumentation V1 is flashed.
- TX_DONE IRQ timestamp instrumentation is active.
- `startTransmit()` call -> TX_DONE IRQ effective transmit occupancy is telemetered every 16 packets in an otherwise decoder-ignored byte.
- No ACK, retransmission, FHSS, AutoFS, fixed packet mode, FEC, or adaptive buffer has been added yet.

### B receiver
- Instrumentation V1 is flashed.
- Speaker digital attenuation changed from 1/4 to **1/1 full digital level**.
- SX1280 **High Sensitivity Mode is ON**.
- Loss-burst, CRC-burst, IRQ-gap, service/read/re-arm timing instrumentation is active.
- High Sensitivity is the only RF robustness feature added so far beyond the earlier PHY/channel tuning.

## Instrumentation results
### High Sensitivity OFF
Approximate end of baseline log:
- received RF packets: 19,361
- frameLoss: 33
- CRC errors: 22
- loss events: 32
- burst histogram: 31 single-packet events, 1 double-packet event
- max burst: 2
- underrun: 0
- A effective TX occupancy: ~4.018 ms
- B `readData()`: ~1.026 ms
- B `startReceive()` re-arm: ~0.424 ms

### High Sensitivity ON — short comparison
Approximate end:
- received RF packets: 17,253
- frameLoss: 7
- CRC errors: 2
- all observed losses were single-packet events
- max burst: 1
- underrun: 0
- no major scheduler timing change

### High Sensitivity ON — longer run
Approximate end of captured log:
- received RF packets: 50,159
- frameLoss: 71
- CRC errors: 46
- loss events: 70
- 69 single-packet events, 1 double-packet event
- max burst: 2
- underrun: 0
- audible crackle is improved but **not completely eliminated**

Interpretation: current remaining audible artifacts correlate with mostly isolated 5.16 ms audio-frame losses rather than PCM starvation. High Sensitivity materially improves reception but does not replace a reliability layer.

## Research findings already established
The following open-source / standards-derived ideas were examined and are candidates for PR1:
- mLRS: SX1280 FLRC 650/CR1/2, High Sensitivity, AutoFS, deterministic bidirectional timing, FHSS, link-quality tracking.
- ExpressLRS: SX1280 IRQ-driven scheduler, FHSS, telemetry slots, rolling link-quality model.
- Nordic ESB: ACK + bounded automatic retransmission + duplicate suppression.
- Nordic nRF Audio / LE Audio: short playout buffers, presentation deadlines, retransmission-aware buffering, clock-drift control.
- Contiki-NG TSCH: deterministic TX/RX/ACK timeslots and channel hopping.
- WebRTC NetEq: recovery-delay / arrival-delay driven adaptive playout target rather than fixed latency buckets.
- Google liblc3: packet-loss concealment reference and future lower-bitrate codec candidate.
- Semtech SX1280 errata: scheduled/single RX is preferred for a mature link rather than blindly relying on long continuous RX under heavy traffic.

## Important measured constraint
Current RadioLib high-level path is expensive:
- A effective packet TX occupancy: ~4.018 ms
- audio packet period: 5.15625 ms
- B readData: ~1.026 ms
- B re-arm: ~0.424 ms

Therefore it is **not yet proven** that per-packet ACK + retry fits safely using the current high-level RadioLib path. A dedicated TDD turnaround benchmark is required before choosing per-packet ACK vs cumulative SACK.

## Algorithms / changes not yet applied
- AutoFS
- fixed-length FLRC packet mode
- CRC3
- preamble 32
- deterministic TDD scheduler
- ACK / cumulative SACK
- sender retransmission cache
- duplicate suppression
- playout deadlines
- bounded ARQ / selective retransmission
- FHSS / frequency diversity
- dynamic channel quality map / blacklist
- adaptive buffer based on measured recovery-time distribution
- clock-drift compensation
- improved PLC / crossfade concealment
- application-layer FEC / redundancy
- LC3 codec benchmark

## Agreed next development principle
The user requested that the researched algorithms be tried rather than prematurely choosing only one. They should still be integrated in controlled checkpoints so failures remain attributable and reversible.

Recommended next checkpoint:
1. Preserve High Sensitivity ON.
2. Add/test AutoFS with no other behavioral change.
3. Test fixed packet + longer preamble + CRC3 as a separate PHY-hardening checkpoint.
4. Build an audio-free TDD DATA->ACK turnaround benchmark.
5. Choose ACK cadence from measured P50/P95/P99/max timing.
6. Implement bounded deadline-aware ARQ.
7. Add frequency diversity/FHSS.
8. Add adaptive playout and PLC.
9. Then compare FEC and LC3.
