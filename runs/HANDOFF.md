# PR1 board experiment — handoff (updated 2026-09-30)

Read `runs/PR1_BOARD_REPORT_20260929.md` first (Gates 0–4 results), then
`runs/pr1-auto-20260930/RESULTS.md` (2026-09-30 unattended suite + antenna check).

## 2026-09-30 status (read before measuring anything)
- **Antenna placement dominates loss at bench/indoor RSSI.** Lifting the TX antenna off the surface cut
  loss ~20 % → 0.04 %; vertical TX gave 0.058 % vs 0.33–0.82 % flat. Flat placement is not repeatable.
- **Measurement standard from now on:** both antennas vertical, boards raised off the surface, USB
  cables away from the antennas, placement photographed and noted in the run folder.
- Needs re-measuring under that standard: 20 m LOS baseline, +3 dBm and CR 1/2 A/B (PR #51 flagged).
- Still holds: 150 µs is the minimum-stable-gap candidate; 125 µs timing loss reproduced; 3 h run had
  no resets, 0 scheduler misses, stable IRQ→RX-ready p99 (~1788 µs), USB stalls auto-recovered.
- **Board images now:** baseline RF + boot-only OLED role label (branch `claude/pr1-oled-role-label`,
  also in `claude/pr1-auto-suite-20260930`). RX = rf_rx CR 3/4, TX = rf_tx CR 3/4 gap 150 µs. The OLED
  shows TX / RX; boot log adds `oled_role_label`, `oled_i2c_addr`.
- Host polling is now rate-based (`progress_polls` ≈ 2 per 10k run) on `claude/pr1-auto-suite-20260930`.
- Unattended bench driver: `runs/pr1-auto-20260930/suite.py`; quick no-reset window check:
  `runs/pr1-auto-20260930/antenna_window.py <label> [seconds]`.

## Rules still in force
- Frozen RF baseline `d1b7ec2b1130fd63fb0eb11fd900b0622766f14c`: no firmware/RF/driver change, no optimisation.
- Host-only fixes live on local branch `local/pr1-board-run-20260929` (commits 42218da, 6215328, fe1d8c4).
- Keep folder path short (e.g. `C:\Users\USER\Projects\running-audio`); long paths break git/PlatformIO on Windows.

## Setup on a new machine (laptop)
```
git clone https://github.com/Rudwpahs/running-audio.git C:\Users\USER\Projects\running-audio
cd C:\Users\USER\Projects\running-audio
git switch claude/pr1-auto-suite-20260930   # newest: baseline data + OLED + tuning flags + suite
python -m pip install pyserial
python -m serial.tools.list_ports -v   # RX board MAC e8:06:90:96:83:38 (SER= field), TX b8:f8:62:d9:26:b4
```
Edit `RX_PORT` / `TX_PORT` in `runs/pr1-distance/distance_driver.py` if ports differ.
Boards already hold their firmware: RX = fixed rf_rx image, TX = rf_tx gap 150 us. No reflash needed,
so PlatformIO is only required if firmware must be re-uploaded (`pip install platformio`; build caches
are not in git and would be rebuilt).

## Where we are: Gate 5 distance, gap 150 us, 10k per point
TX runs untethered on a USB power bank; only RX is on the host.

dist | packets | missing | loss | unpolled-window loss | CRC bad | RSSI
---|---|---|---|---|---|---
0.5 m | 10382 | 31 | 0.30% | 0.23% | 12 | -49
1 m | 10311 | 21 | 0.20% | 0.09% | 4 | -69
5 m | 10306 | 18 | 0.18% | 0.05% | 2 | -71
10 m | 10310 | 20 | 0.19% | 0.03% | 4 | -74

NLOS points (TX in corridor, RX in room, iPhone Measure):
- 13.6 m, 1 wall: 10312 packets, missing 247 (2.40%), unpolled 1.70%, CRC bad 207 (2.0%), RSSI -88 → RF-limited
  (CRC bad tracks missing; turnaround timing unchanged; tool label "spi" is not meaningful here).
- 21.12 m, ~7 walls: no link (0 crc_good, 3 crc_bad in ~34 s) → run stopped, logs kept.

- 20 m LOS (laptop host, RX on COM5): 10384 packets, missing 1048 (10.09%), unpolled 11.58%, CRC bad 767, RSSI -88.

## 20 m tuning (leaves frozen baseline; see `runs/pr1-20m-tuning/RESULTS.md`)
Branch `claude/pr1-20m-tuning-20260929`. Build-flag profile, defaults = frozen.
+3 dBm (PA-board max) → 2.2% (confounded by TX re-placement); +3 dBm + CR 1/2 → 0.8–0.9% at RSSI -76
(clean same-placement comparison). Next: 650 kbps, 260 kbps, frequency.
Both boards are back on baseline (0 dBm, CR 3/4, gap 150): RX reflashed from the laptop, TX from the
desktop (frozen prebuild tx-150us, sha256 4f990a2ba3f5…); boot details in RESULTS.md.

Laptop setup notes: `python` is the Store stub here; PlatformIO + pyserial live in
`C:\Users\foodl\pio-venv` (use its `python.exe`). Build dirs `C:\Users\foodl\pio-build\<role>-<profile>`.
Ports are found by MAC in `tune_driver.py`; RX showed as COM5, TX as COM6.

Next points (laptop, so RX can move): LOS 20 m, 50 m, 100 m, max; then 1-wall at other distances /
door open vs closed. Distance is operator-estimated (record method: paces ≈0.7 m, tiles, app).

## Commands per distance point
```
# only if TX is on the host (validates fresh TX boot, role tx, gap 150):
python runs/pr1-distance/distance_driver.py pre <dist_m> 150
# TX on power bank at <dist_m>, RX on host:
python runs/pr1-distance/distance_driver.py run <dist_m> 150 10000
```
If TX cannot be re-validated at a point, copy the last `tx_pre_move.log` into `runs/pr1-distance/<dist>m/gap-150us/` with a `# NOTE:` first line saying so (see 10m).

## Known measurement caveats
- Each host progress poll (`t`) costs ≈1 packet at 150 us; report the unpolled-window loss alongside the tool loss.
- scheduler_misses is unavailable while TX is untethered (recorded null with reason).
- USB-Serial/JTAG output may stall after ~200 s; the controller reopens the port without reset (`PR1_HOST_SERIAL_REOPEN`).
