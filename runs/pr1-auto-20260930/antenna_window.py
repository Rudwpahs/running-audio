"""Antenna-orientation check: loss over a fixed window from two RX snapshots (no board reset).

usage: antenna_window.py <label> [seconds]
Appends one row to antenna_check.csv. Boards keep running the baseline OLED images.
"""
import csv
import sys
import time
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPO / "tools"))
import pr1_experiment_controller as ctl  # noqa: E402
from pr1_board_sweep import _parse_kv_and_telemetry  # noqa: E402
from serial.tools import list_ports  # noqa: E402

HERE = Path(__file__).resolve().parent
label = sys.argv[1]
seconds = float(sys.argv[2]) if len(sys.argv) > 2 else 60.0
port = next(p.device for p in list_ports.comports() if "E8:06:90:96:83:38" in (p.hwid or "").upper())

log = HERE / "antenna_check_rx.log"
rx = ctl._open_serial(port)
try:
    with log.open("a", encoding="utf-8", buffering=1) as fh:
        fh.write(f"# {ctl._utc_now()} window start: {label}\n")
        ctl._drain_stale_input(rx)
        a = _parse_kv_and_telemetry(ctl._snapshot_with_recovery(rx, fh, 1.0))[1]
        time.sleep(seconds)
        b = _parse_kv_and_telemetry(ctl._snapshot_with_recovery(rx, fh, 1.0))[1]
finally:
    rx.close()

span = (b["crc_good"] + b["missing"]) - (a["crc_good"] + a["missing"])
missing = b["missing"] - a["missing"]
crc_bad = b["crc_bad"] - a["crc_bad"]
row = {"utc": ctl._utc_now(), "label": label, "seconds": seconds, "span": span, "missing": missing,
       "loss_pct": round(100 * missing / span, 4) if span else None, "crc_bad": crc_bad,
       "rssi_dbm": b.get("rssi_dbm"), "irq_to_rx_ready_us_p99": b.get("irq_to_rx_ready_us")}
out = HERE / "antenna_check.csv"
new = not out.exists()
with out.open("a", newline="", encoding="utf-8") as fh:
    w = csv.DictWriter(fh, fieldnames=list(row))
    if new:
        w.writeheader()
    w.writerow(row)
print(f"WINDOW {label}: span {span} missing {missing} loss {row['loss_pct']}% crc_bad {crc_bad} "
      f"rssi {row['rssi_dbm']} ready_p99 {row['irq_to_rx_ready_us_p99']}", flush=True)
