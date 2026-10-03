"""Audio lane diagnostic: does the RX audio path raise RF loss? (same TX, same placement)

Arms (RX image only changes; TX keeps running audio_tx, silent phase unless reset):
  A audio_rx        I2S + amplifier on
  B audio_rx_noamp  I2S running, MAX98357A shut down (SD low)
  C plain_rx        same runtime, no audio task, no I2S
For each run: flash RX, hard reset, wait, pull 't' once, parse crc_good / crc_bad / missing.
  python rx_arm_ab.py <order e.g. ABCABC> <seconds> <out_dir>
"""
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
ARMS = {"A": "audio_rx", "B": "audio_rx_noamp", "C": "plain_rx"}
RX_PORT, RX_MAC = "COM3", "e8:06:90:96:83:38"


def flash(env, attempts=3):
    b = BUILD / env
    for attempt in range(attempts):
        out = _flash_once(b)
        if RX_MAC in out.stdout.lower() and out.stdout.count("Hash of data verified") == 4:
            return
        time.sleep(2)  # transient USB serial error: retry
    raise RuntimeError(out.stdout[-400:])


def _flash_once(b):
    return subprocess.run([PY, "-m", "esptool", "--chip", "esp32s3", "--port", RX_PORT, "--baud", "921600",
                          "--before", "default_reset", "--after", "hard_reset", "write_flash", "-z",
                          "--flash_mode", "dio", "--flash_freq", "80m", "--flash_size", "4MB",
                          "0x0", str(b / "bootloader.bin"), "0x8000", str(b / "partitions.bin"),
                          "0xe000", str(BOOT_APP0), "0x10000", str(b / "firmware.bin")],
                         capture_output=True, text=True)


def pull(seconds):
    s = serial.Serial()
    s.port, s.baudrate, s.timeout, s.dtr, s.rts = RX_PORT, 115200, 0.2, False, False
    s.open()
    time.sleep(seconds)
    s.reset_input_buffer()
    s.write(b"t")
    t, buf = time.time(), b""
    while time.time() - t < 4:
        buf += s.read(8192)
    s.close()
    return buf.decode(errors="replace")


def main():
    order, seconds, out_dir = sys.argv[1], float(sys.argv[2]), HERE / sys.argv[3]
    out_dir.mkdir(parents=True, exist_ok=True)
    rows = []
    for i, arm in enumerate(order, 1):
        flash(ARMS[arm])
        text = pull(seconds)
        (out_dir / f"run{i}_{arm}_{ARMS[arm]}.log").write_text(text, encoding="utf-8")
        f = {k: int(v) for k, v in re.findall(r"field=(\w+) value=(-?\d+)", text)}
        audio = re.search(r"PR1A (.*)", text)
        row = {"run": i, "arm": arm, "env": ARMS[arm], "crc_good": f.get("crc_good"), "crc_bad": f.get("crc_bad"),
               "missing": f.get("missing"), "rssi_dbm": f.get("rssi_dbm"),
               "irq_to_rx_ready_us_p99": f.get("irq_to_rx_ready_us_p99"),
               "audio": dict(t.split("=") for t in audio.group(1).split()) if audio else None}
        if row["crc_good"]:
            row["loss_pct"] = round(100 * row["missing"] / (row["crc_good"] + row["missing"]), 3)
            row["crc_bad_pct"] = round(100 * row["crc_bad"] / (row["crc_good"] + row["missing"]), 3)
        rows.append(row)
        print(row, flush=True)
    (out_dir / "summary.json").write_text(json.dumps(rows, indent=2), encoding="utf-8")


if __name__ == "__main__":
    main()
