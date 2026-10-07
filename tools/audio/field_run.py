"""Field run for the PR1 audio prototype. Needs only Python 3 and pyserial (pip install pyserial).

The RX board (with the speaker) is plugged into this computer. The TX board is on a power bank, silent,
at the test position. For each run:

    python field_run.py "5 m LOS"            # 90 s window (default)
    python field_run.py "5 m body" --window 90
    python field_run.py "40 m" --silent --window 90   # long distance: TX placed beforehand, nobody presses BOOT
    python field_run.py "1 m ref" --port COM5   # only if the port is not found automatically

What happens: the script resets the RX (counters start at 0), waits for it to boot, prints GO, waits the window with
no serial traffic, then reads the RX counters once. Between GO and the end of the window the operator presses the
TX BOOT button ONCE (the clip plays twice, about 28 s) and listens. A short result is printed and saved under
field_results/<label>/ (summary.json + the raw pull). Nothing is changed on the boards.
"""
import json
import re
import sys
import time
from pathlib import Path

import serial
from serial.tools import list_ports


def find_port():
    ports = [p.device for p in list_ports.comports() if p.vid == 0x303A]
    if len(ports) != 1:
        sys.exit(f"found {len(ports)} Espressif USB ports {ports}; plug only the RX board in, or pass --port")
    return ports[0]


def open_port(name):
    s = serial.Serial()
    s.port, s.baudrate, s.timeout, s.write_timeout = name, 115200, 0.2, 1.0
    s.dtr = False  # opening must not pulse reset/strap lines
    s.rts = False
    s.open()
    return s


def reset(s):
    s.dtr = False  # Windows only sends the line state when DTR is written; DTR must stay low at release
    s.rts = True
    s.dtr = False
    time.sleep(0.10)
    s.rts = False
    s.dtr = False
    time.sleep(0.35)


def read_for(s, seconds, stop_text=None):
    t, buf = time.time(), ""
    while time.time() - t < seconds:
        buf += s.read(4096).decode(errors="replace")
        if stop_text and stop_text in buf:
            break
    return buf


def main():
    a = sys.argv[1:]
    if not a or a[0].startswith("--"):
        sys.exit(__doc__)
    label = a[0]
    opt = lambda k, d: a[a.index(k) + 1] if k in a else d
    window = float(opt("--window", 90))
    port = opt("--port", None) or find_port()
    out = Path("field_results") / re.sub(r"[^\w.-]+", "_", label)
    out.mkdir(parents=True, exist_ok=True)

    s = open_port(port)
    reset(s)
    boot = read_for(s, 8.0, "PR1_RUNTIME_LIVE_READY")
    ok = "PR1_RUNTIME_LIVE_READY" in boot
    print(f"RX on {port}: boot {'OK' if ok else 'NOT SEEN (check the RX board)'}")
    if not ok:
        sys.exit(1)
    t0 = time.time()
    if "--silent" in a:
        print(time.strftime("%H:%M:%S"), f"GO (silent run). Do NOT press BOOT; leave the TX alone. Window {window:.0f} s.", flush=True)
    else:
        print(time.strftime("%H:%M:%S"), f"GO. Press the TX BOOT button ONCE now, then listen. Window {window:.0f} s.", flush=True)
    while time.time() - t0 < window:
        time.sleep(min(15.0, max(0.1, window - (time.time() - t0))))
        left = window - (time.time() - t0)
        if left > 1:
            print(f"  {left:.0f} s left", flush=True)
    s.reset_input_buffer()
    s.write(b"t")
    text = read_for(s, 4.0)
    s.close()
    (out / "rx_pull.log").write_text(text, encoding="utf-8")

    f = {k: int(v) for k, v in re.findall(r"field=(\w+) value=(-?\d+)", text)}
    m = re.search(r"PR1A (.*)", text)
    au = {k: int(v) for k, v in (t.split("=") for t in m.group(1).split())} if m else None
    res = {"label": label, "window_s": round(time.time() - t0, 1), "crc_good": f.get("crc_good"),
           "crc_bad": f.get("crc_bad"), "missing": f.get("missing"), "rssi_dbm": f.get("rssi_dbm")}
    flags = []
    if f.get("crc_good") is not None:
        total = f["crc_good"] + f["missing"]
        res["loss_pct"] = round(100 * f["missing"] / max(1, total), 3)
        expected = window * 330  # ~330 frames/s at the 150 us gap
        if total < 0.5 * expected:
            flags.append("FEW FRAMES: TX lost power or link (check the power bank)")
    else:
        flags.append("NO COUNTERS READ")
    if au:
        played = au["frames"] / 188
        res["concealed_blocks"] = au["missing_blocks"]
        res["concealed_pct"] = round(100 * au["missing_blocks"] / max(1.0, played - au["startup_blocks"]), 3)
        res["resets"], res["i2s_errors"], res["queue_dropped"] = au["resets"], au["write_errors"], au["queue_dropped"]
        if au["resets"] or au["write_errors"] or au["queue_dropped"]:
            flags.append("STOP RULE: reset / I2S error / queue drop")
    else:
        flags.append("NO AUDIO COUNTERS (is the RX flashed with audio_rx?)")
    res["flags"] = flags
    (out / "summary.json").write_text(json.dumps(res, indent=2), encoding="utf-8")
    print("\nRESULT " + json.dumps(res))


if __name__ == "__main__":
    main()
