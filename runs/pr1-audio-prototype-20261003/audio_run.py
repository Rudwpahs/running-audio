"""Audio lane measurement run: flash (optional) + reset RX then TX, play, pull stats once at the end.

  python audio_run.py <out_dir> <seconds> [--tx-env audio_tx_5min] [--rx-env audio_rx] [--no-flash] [--note "..."]

No serial traffic during the run (a 't' pull stalls the radio loop); one pull per board at the end.
Writes rx_pull.log, tx_pull.log, summary.json (RF counters, audio counters, image sha256).
"""
import hashlib
import json
import os
import re
import subprocess
import sys
import time
from pathlib import Path

import serial

HERE = Path(__file__).resolve().parent
BUILD = HERE.parents[1] / "firmware" / "t3s3_sx1280_runtime" / ".pio" / "build"
USERP = Path(os.environ["USERPROFILE"])
PY = str(USERP / ".platformio/penv/Scripts/python.exe")
BOOT_APP0 = USERP / ".platformio/packages/framework-arduinoespressif32/tools/partitions/boot_app0.bin"
BOARDS = {"rx": ("COM3", "e8:06:90:96:83:38"), "tx": ("COM4", "b8:f8:62:d9:26:b4")}


def esptool(port, *args):
    return subprocess.run([PY, "-m", "esptool", "--chip", "esp32s3", "--port", port, *args],
                          capture_output=True, text=True).stdout


def flash(role, env):
    port, mac = BOARDS[role]
    b = BUILD / env
    for _ in range(3):
        out = esptool(port, "--baud", "921600", "--before", "default_reset", "--after", "no_reset", "write_flash", "-z",
                      "--flash_mode", "dio", "--flash_freq", "80m", "--flash_size", "4MB",
                      "0x0", str(b / "bootloader.bin"), "0x8000", str(b / "partitions.bin"),
                      "0xe000", str(BOOT_APP0), "0x10000", str(b / "firmware.bin"))
        if mac in out.lower() and out.count("Hash of data verified") == 4:
            return hashlib.sha256((b / "firmware.bin").read_bytes()).hexdigest()
        time.sleep(2)
    raise RuntimeError(f"flash {role} failed: {out[-300:]}")


def reset(role):
    esptool(BOARDS[role][0], "--before", "default_reset", "--after", "hard_reset", "chip_id")


def open_port(role):
    s = serial.Serial()
    s.port, s.baudrate, s.timeout, s.dtr, s.rts = BOARDS[role][0], 115200, 0.2, False, False
    s.open()
    return s


def pull(s):
    s.reset_input_buffer()
    s.write(b"t")
    t, buf = time.time(), b""
    while time.time() - t < 4:
        buf += s.read(8192)
    return buf.decode(errors="replace")


def main():
    a = sys.argv[1:]
    out_dir, seconds = HERE / a[0], float(a[1])
    opt = lambda k, d: a[a.index(k) + 1] if k in a else d
    tx_env, rx_env = opt("--tx-env", "audio_tx"), opt("--rx-env", "audio_rx")
    out_dir.mkdir(parents=True, exist_ok=True)
    sha = {}
    if "--no-flash" not in a:
        sha["rx"] = flash("rx", rx_env)
        sha["tx"] = flash("tx", tx_env)
    reset("rx")
    time.sleep(3)
    reset("tx")
    started = time.time()
    rx, tx = open_port("rx"), open_port("tx")
    time.sleep(seconds)
    rx_text, tx_text = pull(rx), pull(tx)
    elapsed = time.time() - started
    rx.close(); tx.close()
    (out_dir / "rx_pull.log").write_text(rx_text, encoding="utf-8")
    (out_dir / "tx_pull.log").write_text(tx_text, encoding="utf-8")
    f = {k: int(v) for k, v in re.findall(r"field=(\w+) value=(-?\d+)", rx_text)}
    ft = {k: int(v) for k, v in re.findall(r"field=(\w+) value=(-?\d+)", tx_text)}
    audio = re.search(r"PR1A (.*)", rx_text)
    au = {k: int(v) for k, v in (t.split("=") for t in audio.group(1).split())} if audio else None
    s = {"out_dir": a[0], "seconds_requested": seconds, "seconds_elapsed": round(elapsed, 1), "tx_env": tx_env,
         "rx_env": rx_env, "image_sha256": sha, "note": opt("--note", None),
         "rf": {k: f.get(k) for k in ("crc_good", "crc_bad", "missing", "rssi_dbm", "irq_to_rx_ready_us_p99",
                                      "spi_duration_us_p99", "rx_rearm_us_p99")},
         "tx": {k: ft.get(k) for k in ("scheduler_misses", "tx_done", "tx_started")}, "audio": au}
    if f.get("crc_good"):
        total = f["crc_good"] + f["missing"]
        s["rf"]["loss_pct"] = round(100 * f["missing"] / total, 3)
    if au:
        played = au["frames"] / 188  # blocks written to I2S (incl. start-up silence)
        s["audio"]["concealed_pct"] = round(100 * au["missing_blocks"] / max(1.0, played - au["startup_blocks"]), 3)
        s["audio"]["i2s_seconds"] = round(au["frames"] / 32000, 1)
    (out_dir / "summary.json").write_text(json.dumps(s, indent=2), encoding="utf-8")
    print(json.dumps(s, indent=1))


if __name__ == "__main__":
    main()
