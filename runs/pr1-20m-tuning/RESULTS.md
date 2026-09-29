# 20 m LOS link-budget tuning — 2026-09-29

Leaves the frozen RF baseline on purpose (operator request): profile set by build
flags (`PR1_FREQ_KHZ`, `PR1_FLRC_BITRATE_KBPS`, `PR1_FLRC_CR`, `PR1_TX_OUTPUT_DBM`),
defaults unchanged = frozen baseline. Driver: `tune_driver.py`.

Conditions: 20 m LOS (operator-reported, measuring method not recorded), TX untethered
on USB power bank, RX on host laptop, gap 150 us, 10k packets per run, 2404 MHz.
TX was carried back to the host for every reflash and replaced at the same spot.
No 2.4 GHz Wi-Fi visible from the host (only one 5 GHz hotspot).

output_dbm | CR | kbps | run | packets | missing | loss | unpolled-window loss | CRC bad | RSSI
---|---|---|---|---|---|---|---|---|---
0 (baseline) | 3/4 | 1300 | `../pr1-distance/20m/gap-150us` | 10384 | 1048 | 10.09% | 11.58% | 767 | -88
+3 | 3/4 | 1300 | `f2404-br1300-cr3-p3/partial/20260929T130401Z` | 10435 | 232 | 2.22% | 2.27% | 170 | -75
+3 | 3/4 | 1300 | `f2404-br1300-cr3-p3` (repeat) | 10446 | 230 | 2.20% | 2.14% | 184 | -75
+3 | 1/2 | 1300 | `f2404-br1300-cr2-p3/partial/20260929T131631Z` | 10267 | 94 | 0.92% | 0.89% | 71 | -76
+3 | 1/2 | 1300 | `f2404-br1300-cr2-p3` (repeat) | 10284 | 82 | 0.80% | 0.71% | 61 | -76

`partial/` holds the first of two back-to-back runs: the driver archives any previous
attempt before a rerun. Both runs completed (target reached); they are repeats, not failures.

## Reading
- 0 → +3 dBm is **confounded by TX re-placement**: the TX was carried back for reflash and
  put down again, and there is no 0 dBm rerun at the new spot. RSSI rose ~13 dB for a
  3 dB power step, so placement/fading likely contributed most of the 10% → 2.2% change.
- CR 3/4 → 1/2 is a **clean same-placement comparison** (same spot, RSSI -75/-76):
  loss 2.2% → 0.8–0.9%, CRC bad drops with it. This is a coding-gain effect.
- Repeats agree within ~0.1 pt, so run-to-run variance at a fixed placement is small.
- Unpolled-window loss ≈ tool loss throughout: losses are RF, not host-poll artifacts.
- +3 dBm is the ceiling on this board (SX1280 PA variant; LilyGo limit -18..+3 dBm chip output).

## Not done / next
- 650 kbps + CR 1/2 (RX was flashed with it, TX was not: `f2404-br650-cr2-p3/rx_boot.log`
  only, no run). Then 260 kbps and frequency moves if loss is still above target.
- Placement repeatability (move TX away and back) to separate fading from settings.

## End-of-session restore to baseline (0 dBm, CR 3/4, 1300 kbps, 2404 MHz, gap 150)
- RX (e8:06:90:96:83:38, laptop COM5): reflashed from this branch with default flags
  (= frozen values) and boot-validated by `tune_driver.py`: `f2404-br1300-cr3-p0/rx_boot.log`.
- TX (b8:f8:62:d9:26:b4): restored from the desktop PC (COM4) by the desktop session, not
  from this laptop, so no TX boot log is in this folder. As reported by that session: frozen
  prebuild `rf_tx_compile` tx-150us image (`runs/pr1-board-test`, branch
  `claude/pr1-board-run-20260929`), firmware.bin sha256 prefix 4f990a2ba3f5, written with
  esptool, hash verified; fresh boot showed PR1_RUNTIME_BOOT, runtime_role=tx, rf_enabled=1,
  sx1280_spi_hz=2000000, frequency_mhz=2404.000, bitrate_kbps=1300, coding_rate=3,
  output_dbm=0, tx_gap_us=150, packet_bytes=116, adaptive_layers=off, PR1_RUNTIME_LIVE_READY.
