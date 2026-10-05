"""V0 distance / body-block run. The RX stays on the PC; the TX runs on a power bank (no USB).

  python distance_run.py <out_dir> --label "5 m LOS" [--window 120] [--flash]

Procedure (the operator does the physical part, this script does the rest):
  1. TX already powered on its power bank at the test position (it sends silent blocks, no sound).
  2. Run this script. It resets the RX (RF/audio counters start at 0) and prints START.
  3. The operator presses the TX BOOT button once (the clip plays twice, ~28 s) and listens.
  4. After <window> seconds the script pulls the RX counters once and writes summary.json.
No serial traffic during the window (a pull stalls the radio loop). The TX cannot be pulled (no USB), so
the loss figure comes from the RX counters only. The window also covers the silent lead-in/tail; RF
conditions are the same for silent and clip blocks (payload is scrambled).

--flash first flashes audio_rx on the RX and audio_tx150 on the TX, so the TX must still be on the PC
USB at that point; move it to the power bank afterwards.
"""
import json
import re
import sys
import time

import audio_run as a

args = sys.argv[1:]
opt = lambda k, d: args[args.index(k) + 1] if k in args else d
out_dir = a.HERE / args[0]
label, window = opt("--label", ""), float(opt("--window", 120))
out_dir.mkdir(parents=True, exist_ok=True)
sha = {}
if "--flash" in args:
    sha["rx"], sha["tx"] = a.flash("rx", "audio_rx"), a.flash("tx", "audio_tx150")
    a.reset("tx")
    print("flashed; move the TX to the power bank, then run again without --flash", flush=True)
    sys.exit(0)
a.reset("rx")
rx = a.open_port("rx")
t0 = time.time()
print(time.strftime("%H:%M:%S"), "START: RX reset. Press the TX BOOT button once now.", flush=True)
time.sleep(window)
text = a.pull(rx)
rx.close()
(out_dir / "rx_pull.log").write_text(text, encoding="utf-8")
f = {k: int(v) for k, v in re.findall(r"field=(\w+) value=(-?\d+)", text)}
m = re.search(r"PR1A (.*)", text)
au = {k: int(v) for k, v in (t.split("=") for t in m.group(1).split())} if m else None
rf = {k: f.get(k) for k in ("crc_good", "crc_bad", "missing", "rssi_dbm", "scheduler_misses")}
if f.get("crc_good"):
    rf["loss_pct"] = round(100 * f["missing"] / (f["crc_good"] + f["missing"]), 3)
if au:
    played = au["frames"] / 188
    au["concealed_pct"] = round(100 * au["missing_blocks"] / max(1.0, played - au["startup_blocks"]), 3)
s = {"label": label, "window_s": round(time.time() - t0, 1), "rf": rf, "audio": au, "image_sha256": sha,
     "note": "RX counters only; TX not pullable on a power bank"}
(out_dir / "summary.json").write_text(json.dumps(s, indent=2), encoding="utf-8")
print(json.dumps(s, indent=1))
