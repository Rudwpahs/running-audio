# PR1 SX1280 two-board frozen-baseline report — 2026-09-29

## 1. Environment
- Repo: Rudwpahs/running-audio. main `0df1cfb`; PR #46 head = frozen `d1b7ec2b1130fd63fb0eb11fd900b0622766f14c` (OPEN); PR #47 head `b7c4dee` (OPEN).
- Run source: local branch `local/pr1-board-run-20260929` = `b7c4dee` + host-only fixes `42218da`, `6215328`, `fe1d8c4` (not pushed). Firmware tree identical to frozen SHA.
- RX = COM3 (ESP32-S3 MAC e8:06:90:96:83:38), TX = COM4 (MAC b8:f8:62:d9:26:b4). Boards: 2.
- Baseline verified from fresh boot blocks on every run: FLRC 2404.000 MHz, 1300 kbps, CR 3, 0 dBm, 116 B, SPI 2 MHz, adaptive off, role rx/tx, requested tx_gap_us.
- Time: 2026-09-29 18:32–19:02 KST. TX–RX distance for all Gate 1–4 runs: 50 cm (reported by operator after the runs).

## 2. Frozen sweep (1k, `runs/pr1-board-test`)
gap_us | packets | missing | loss % | CRC good | CRC bad | RSSI | qmax | sched miss | irq→spi | spi | rx proc | spi→rearm | rearm | irq→ready | class
---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---
5000 | 1172 | 5 | 0.427 | 1167 | 0 | -49 | 1 | 0 | 6 | 1248 | 1250 | 136 | 421 | 1794 | spi
1000 | 1374 | 3 | 0.218 | 1371 | 1 | -48 | 1 | 0 | 6 | 1250 | 1252 | 137 | 416 | 1795 | spi
500 | 1477 | 4 | 0.271 | 1473 | 1 | -48 | 1 | 0 | 6 | 1251 | 1253 | 138 | 417 | 1794 | spi
300 | 1530 | 4 | 0.261 | 1526 | 0 | -49 | 1 | 0 | 5 | 1251 | 1253 | 138 | 417 | 1796 | spi
250 | 1536 | 6 | 0.391 | 1530 | 0 | -49 | 1 | 0 | 13 | 1246 | 1248 | 137 | 421 | 1794 | spi
225 | 1550 | 3 | 0.194 | 1547 | 0 | -53 | 1 | 0 | 6 | 1245 | 1247 | 136 | 422 | 1796 | spi
200 | 1554 | 5 | 0.322 | 1549 | 0 | -53 | 1 | 0 | 6 | 1249 | 1250 | 138 | 418 | 1793 | spi
175 | 1566 | 5 | 0.319 | 1561 | 1 | -48 | 1 | 0 | 6 | 1249 | 1251 | 137 | 417 | 1795 | spi
150 | 1569 | 6 | 0.382 | 1563 | 0 | -53 | 1 | 0 | 6 | 1245 | 1247 | 137 | 420 | 1792 | spi
125 | 1573 | 6 | 0.381 | 1567 | 1 | -47 | 1 | 0 | 6 | 1247 | 1249 | 138 | 421 | 1794 | spi
0 | 1265 | 632 | 49.960 | 633 | 0 | -47 | 1 | 0 | 6 | 1245 | 1247 | 137 | 420 | 1794 | spi

Timing = firmware p99 (DurationWindow<64>), µs. p50/p95: not measurable on the frozen baseline (raw samples not exposed).

## 3. Transition (tool rule, 1k)
- Only material transition: 125 → 0 µs (+49.58 pp). Marked for 10k: 125, 0.

## 4. 10k / 100k (Gate 4)
gap_us | run | packets | missing | loss % | CRC bad | RSSI | qmax | sched | irq→spi | spi | rx proc | spi→rearm | rearm | irq→ready | unpolled-window loss
---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---
0 | reval 10k | 10556 | 5279 | 50.009 | 3 | -49 | 1 | 0 | 6 | 1247 | 1249 | 137 | 422 | 1796 | 50.0% (strict alternation)
125 | reval 10k | 10413 | 109 | 1.047 | 5 | -48 | 1 | 0 | 6 | 1247 | 1248 | 137 | 420 | 1795 | 1.07% (69/6426)
150 | diag 10k | 10382 | 31 | 0.299 | 12 | -49 | 1 | 0 | 11 | 1246 | 1248 | 137 | 421 | 1792 | 0.23% (15/6432)
175 | diag 10k | 10359 | 21 | 0.203 | 3 | -49 | 1 | 0 | 6 | 1249 | 1251 | 137 | 417 | 1792 | 0.08% (5/6442)
5000 | diag 10k (control) | 10166 | 10 | 0.098 | 12 | -53 | 1 | 0 | 13 | 1249 | 1251 | 137 | 417 | 1794 | 0.10% (7/6785)
150 | diag 100k | 100251 | 200 | 0.199 | 36 | -50 | 1 | 0 | 6 | 1246 | 1248 | 137 | 421 | 1796 | 0.090% (58/64310)

- Tool rule on combined 10k set: material 150 → 125 (0.30 → 1.05%, threshold 0.5 pp) and 125 → 0.
- 150 µs 100k over time (10k windows after polling began): 0.44 / 0.39 / 0.38 / 0.37 % — no upward drift; elevated vs unpolled window ≈ 1 loss per progress poll.
- "Unpolled window" = cumulative-counter delta from RX start to the first progress snapshot (firmware counters, no host reconstruction).

## 5. Minimum stable gap (provisional)
- Candidate: **150 µs** (100k: 0.20% incl. poll artifact, 0.09% unpolled). 125 µs degrades (~1%) independent of polling; 0 µs collapses to 50%.
- Not confirmed: one session, one bench placement, one RF environment; 150 is one step above measured degradation.

## 6. Bottleneck evidence (candidates, not causal proof)
- Scheduler: IRQ→SPI p99 5–13 µs, scheduler_misses 0 everywhere → not supported.
- SPI: p99 1245–1251 µs at every gap, ~70% of IRQ→ready; 116 B at 2 MHz is ~464 µs wire time → ~2.7× overhead (cause unmeasured).
- RX processing (SPI end→rearm): 136–138 µs, constant → minor.
- Rearm: 416–422 µs, constant → second-largest.
- Total IRQ→RX-ready p99 1792–1796 µs, constant across gaps; the tool's receiver_saturation rule (turnaround rise ≥25%) is not triggered.
- Mechanism consistent with data: fixed ~1.8 ms receiver turnaround vs shrinking inter-packet idle. 0 µs gives strict alternation (every other packet lost). TX period from sequence advance: ~2.74 ms @0 µs, ~2.86 ms @125 µs, ~7.70 ms @5000 µs. p99-only telemetry cannot show whether the ~1% at 125 µs is the >p99 tail.
- RF/CRC: RSSI -47…-53 dBm stable, CRC bad ≤0.12% and not gap-correlated → does not explain the transition.
- Past-data comparison: the earlier "≤150 µs ≈50% collapse" was not reproduced; collapse now only at 0 µs, mild degradation at 125 µs.

## 6b. Gate 5 distance / NLOS (gap 150 µs, 10k per point, frozen baseline)
TX untethered on a USB power bank (scheduler_misses unavailable, recorded null). Distances operator-reported.

condition | packets | missing | loss % | unpolled-window loss | CRC bad | RSSI dBm | irq→ready p99
---|---|---|---|---|---|---|---
0.5 m LOS (Gate 4 run) | 10382 | 31 | 0.299 | 0.23% | 12 | -49 | 1792
1 m LOS | 10311 | 21 | 0.204 | 0.09% | 4 | -69 | 1794
5 m LOS | 10306 | 18 | 0.175 | 0.05% | 2 | -71 | 1794
10 m LOS | 10310 | 20 | 0.194 | 0.03% | 4 | -74 | 1792
13.6 m, 1 wall (NLOS) | 10312 | 247 | 2.395 | 1.70% | 207 | -88 | 1792
20 m LOS corridor (laptop session) | 10384 | 1048 | 10.09 | 11.58% | 767 | -88 (-94 mid-run) | –
21.12 m, ~7 walls (NLOS) | no link | – | – | – | 3 in ~34 s | – | –

- Receiver turnaround timing is unchanged at every point; loss beyond 10 m is RF-limited (CRC bad tracks missing).
- 1 m → 10 m RSSI drop is small, but 10 m → 20 m drops 14–20 dB (free space ≈ 6 dB): placement/antenna
  orientation (horizontal whips, metal radiator grille, cable tied to antenna) is the leading suspect, not proven.
- The 20 m LOS point came from the laptop session (branch `claude/pr1-20m-tuning-20260929`), same frozen images.
- Output/coding A/B at 20 m is on that tuning branch and is **not** part of this baseline; its first step
  (+3 dBm, 2.2% loss, RSSI -75) is confounded by a TX re-placement (RSSI +13 dB for a 3 dB change).

## 7. Host-harness issues found and fixed (host only)
1. Relative paths → TX builds failed (pio chdir). 2. RX reset before TX upload let previous-gap packets seed RX sequence (loss under-count). 3. Windows usbser RTS-only change never reached the board (esptool DTR rewrite workaround); DTR must be low at release. 4. Boot block lost before port open → reset with port open, require fresh BOOT+LIVE_READY. 5. Snapshot lines split across USB reads / reader stopped early. 6. USB-Serial/JTAG output stalls after ~200 s (RX keeps running) → reopen port without reset.
- Open measurement caveat: progress polling itself causes ≈1 loss per poll at short gaps. Future runs should minimise polls (e.g. rate-based single stop snapshot) — host-only change, not yet applied.

## 8. Follow-up / optimisation candidates (not implemented)
- Gate 5 open items: controlled 20 m re-baseline (both antennas vertical, off metal, cable untied), 50 m / 100 m / max LOS.
- Not started: Gate 6 body block, Gate 7 obstacle/locker door (only two wall points so far), Gate 8 movement,
  Gate 9–10 real audio / long audio, Gate 11 park, Gate 12 locker, Gate 13 Bluetooth comparison.
- Evidence-backed candidates only if a gap < 150 µs is required: SPI read path (dominant ~70% of turnaround), then rearm (~23%). No other candidate is supported by this data.
