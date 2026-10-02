"""PR1-DART layered board campaign (issue #52), host orchestration only.

  build                         build every image used by Gates A/B (parallel)
  run <gate> <gap_us> <target>  flash (if needed) + reset both + capture + hop stats
                                gate: A (frozen fixed-channel images) | B (static AFH)
  summary                       rebuild matrix.csv / matrix.json from run results

Gate A uses the frozen 2026-09-29 prebuilt images (runs/pr1-board-test/build), byte-identical
to the baseline. Gate B images are built from this branch with -D PR1_ENABLE_AFH=1 only.
"""
import csv
import json
import os
import subprocess
import sys
import time
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPO / "tools"))

import pr1_experiment_controller as ctl  # noqa: E402
from pr1_board_sweep import FROZEN_BASELINE, _parse_kv_and_telemetry  # noqa: E402
from serial.tools import list_ports  # noqa: E402

HERE = Path(__file__).resolve().parent
PROJECT = REPO / "firmware" / "t3s3_sx1280_runtime"
USERP = Path(os.environ["USERPROFILE"])
PIO = str(USERP / ".platformio/penv/Scripts/pio.exe")
PY = str(USERP / ".platformio/penv/Scripts/python.exe")
BOOT_APP0 = USERP / ".platformio/packages/framework-arduinoespressif32/tools/partitions/boot_app0.bin"
MAC = {"rx": "E8:06:90:96:83:38", "tx": "B8:F8:62:D9:26:B4"}
FROZEN_BUILD = REPO / "runs" / "pr1-board-test" / "build"
GAPS_B = [5000, 1000, 150]
PLACEMENT = ("2026-10-03 operator photo placement_20261003.jpg: RX on desk top (antenna ~vertical, iron and "
             "bottles nearby), TX on white box below (antenna vertical). Unchanged for all gates.")


def image_dir(gate: str, role: str, gap: int | None) -> Path:
    env = "rf_tx_compile" if role == "tx" else "rf_rx_compile"
    if gate == "A":
        return FROZEN_BUILD / ("rx" if role == "rx" else f"tx-{gap}us") / env
    return HERE / "build" / (f"B-{role}" + (f"-{gap}us" if role == "tx" else "")) / env


def render_ini(role: str, gap: int | None) -> Path:
    text = (PROJECT / "platformio.ini").read_text(encoding="utf-8")
    if role == "tx":
        text = text.replace("-D PR1_TX_GAP_US=5000", f"-D PR1_TX_GAP_US={gap}\n    -D PR1_ENABLE_AFH=1", 1)
    else:
        text = text.replace("-D PR1_RUNTIME_ROLE=2", "-D PR1_RUNTIME_ROLE=2\n    -D PR1_ENABLE_AFH=1", 1)
    path = HERE / "generated" / (f"B-{role}" + (f"-{gap}us" if role == "tx" else "") + ".ini")
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text, encoding="utf-8")
    return path


def build_one(item) -> tuple[str, int]:
    role, gap = item
    ini = render_ini(role, gap)
    out = image_dir("B", role, gap)
    env = dict(os.environ, PLATFORMIO_BUILD_DIR=str(out.parent))
    log = out.parent.parent / f"{out.parent.name}.log"
    log.parent.mkdir(parents=True, exist_ok=True)
    with log.open("w", encoding="utf-8") as fh:
        rc = subprocess.run([PIO, "run", "-d", str(PROJECT), "-c", str(ini), "-e", out.name],
                            env=env, stdout=fh, stderr=subprocess.STDOUT).returncode
    return out.parent.name, rc


def cmd_build() -> None:
    items = [("rx", None)] + [("tx", g) for g in GAPS_B]
    with ThreadPoolExecutor(max_workers=2) as pool:
        for name, rc in pool.map(build_one, items):
            print(f"BUILD {name} rc={rc}", flush=True)
            if rc != 0:
                raise SystemExit(f"build failed: {name}")


def port_of(role: str) -> str:
    for p in list_ports.comports():
        if MAC[role] in (p.hwid or "").upper():
            return p.device
    raise RuntimeError(f"{role} board ({MAC[role]}) not connected")


STATE_FILE = HERE / "flashed.json"


def flash(gate: str, role: str, gap: int | None) -> dict:
    b = image_dir(gate, role, gap)
    fw = b / "firmware.bin"
    key = f"{gate}:{b}"
    state = json.loads(STATE_FILE.read_text()) if STATE_FILE.exists() else {}
    sha = ctl._sha256(fw)
    if state.get(role, {}).get("key") == key and state[role].get("sha256") == sha:
        return state[role]
    out = subprocess.run([PY, "-m", "esptool", "--chip", "esp32s3", "--port", port_of(role), "--baud", "921600",
                          "--before", "default_reset", "--after", "hard_reset", "write_flash", "-z",
                          "--flash_mode", "dio", "--flash_freq", "80m", "--flash_size", "4MB",
                          "0x0", str(b / "bootloader.bin"), "0x8000", str(b / "partitions.bin"),
                          "0xe000", str(BOOT_APP0), "0x10000", str(fw)], capture_output=True, text=True)
    if out.returncode != 0 or out.stdout.count("Hash of data verified") != 4 or MAC[role].lower() not in out.stdout.lower():
        raise RuntimeError(f"flash failed {role} {b}: {out.stdout[-300:]}{out.stderr[-300:]}")
    state[role] = {"key": key, "image": str(b.relative_to(REPO)), "sha256": sha, "flashed_utc": ctl._utc_now()}
    STATE_FILE.write_text(json.dumps(state, indent=2), encoding="utf-8")
    print(f"FLASH {role} <- {b.relative_to(REPO)} sha256 {sha[:12]}", flush=True)
    return state[role]


def pull(ser, fh, command: bytes, end_marker: str | None, timeout_s: float = 3.0) -> list[str]:
    """Send a pull command and log its reply (hop stats / ring dump)."""
    ctl._drain_stale_input(ser, settle_s=0.05)
    ser.write(command)
    ser.flush()
    deadline = time.monotonic() + timeout_s
    pending = bytearray()
    lines: list[str] = []
    idle = 0
    while time.monotonic() < deadline:
        line = ctl._read_complete_line(ser, pending)
        if line is None:
            idle += 1
            if end_marker is None and lines and idle >= 3:
                break
            continue
        idle = 0
        fh.write(line + "\n")
        fh.flush()
        lines.append(line)
        if end_marker and line.strip().startswith(end_marker):
            break
    return lines


def parse_hop(lines: list[str]) -> dict:
    out: dict = {}
    for ln in lines:
        if ln.startswith("PR1H "):
            for tok in ln.split()[1:]:
                k, _, v = tok.partition("=")
                out[k] = int(v) if v.lstrip("-").isdigit() else v
        elif ln.startswith("PR1HC "):
            for tok in ln.split()[1:]:
                k, _, v = tok.partition("=")
                out[f"channel_{k}"] = [int(x) for x in v.split(",")]
    return out


def cmd_run(gate: str, gap: int, target: int) -> None:
    run_id = f"{gate}-gap-{gap}us-{target}"
    run_dir = HERE / f"gate-{gate}" / run_id
    run_dir.mkdir(parents=True, exist_ok=True)
    if (run_dir / "result.json").exists() and json.loads((run_dir / "result.json").read_text())["derived"]["target_reached"]:
        print(f"SKIP {run_id} (complete)", flush=True)
        return
    if (run_dir / "rx.log").exists():
        ctl.archive_partial_attempt(run_dir)
    rx_img = flash(gate, "rx", None)
    tx_img = flash(gate, "tx", gap)
    meta = {
        "schema_version": 1, "run_id": run_id, "created_utc": ctl._utc_now(), "gate": gate, "gap_us": gap,
        "target_packets": target, "phase": "revalidate", "firmware_sha": ctl.FROZEN_FIRMWARE_SHA,
        "frozen_baseline": dict(FROZEN_BASELINE),
        "feature_flags": {"PR1_ENABLE_AFH": 1 if gate == "B" else 0, "afh_map": "static all-40" if gate == "B" else None,
                          "channel_quality": 0, "fec": 0, "arq": 0, "phy_ladder": 0, "controller": 0},
        "images": {"rx": rx_img, "tx": tx_img}, "placement": PLACEMENT,
        "files": {"rx_log": "rx.log", "tx_log": "tx.log"},
        "build_identity": ctl.collect_build_identity(PROJECT, source_root=REPO),
        "notes": "issue #52 layered activation",
    }
    (run_dir / "metadata.json").write_text(json.dumps(meta, indent=2), encoding="utf-8")
    period_s = (gap + 2750) / 1e6
    rx = ctl._open_serial(port_of("rx"))
    tx = ctl._open_serial(port_of("tx"))
    hop = {}
    try:
        ctl._drain_stale_input(tx); ctl._pulse_reset(tx)
        ctl._drain_stale_input(rx); ctl._pulse_reset(rx)
        result = ctl.capture_run_from_serial(meta, run_dir, rx, tx, initial_wait_s=0.7 * target * period_s,
                                             settle_s=0.5, max_polls=100)
        if gate == "B":
            with (run_dir / "rx.log").open("a", encoding="utf-8") as rfh, (run_dir / "tx.log").open("a", encoding="utf-8") as tfh:
                hop["rx"] = parse_hop(pull(rx, rfh, b"h", None))
                hop["tx"] = parse_hop(pull(tx, tfh, b"h", None))
                pull(rx, rfh, b"H", "PR1HE_END", timeout_s=6.0)
                pull(tx, tfh, b"H", "PR1HE_END", timeout_s=6.0)
            for role, log in (("rx", run_dir / "rx.log"), ("tx", run_dir / "tx.log")):
                meta_kv, _ = _parse_kv_and_telemetry(log.read_text(encoding="utf-8", errors="replace").splitlines())
                hop[role]["boot_schedule_fp"] = meta_kv.get("afh_schedule_fp")
                hop[role]["boot_session_seed"] = meta_kv.get("afh_session_seed")
                hop[role]["boot_session_id"] = meta_kv.get("afh_session_id")
                hop[role]["boot_map_version"] = meta_kv.get("afh_map_version")
            hop["schedule_fp_match"] = hop["rx"].get("boot_schedule_fp") == hop["tx"].get("boot_schedule_fp")
            result["hop"] = hop
            (run_dir / "result.json").write_text(json.dumps(result, indent=2), encoding="utf-8")
    finally:
        tx.close(); rx.close()
    m = result["metrics"]
    line = (f"RUN {run_id} packets={result['packet_count']} missing={m['missing']} "
            f"loss={100 * result['derived']['loss_rate']:.3f}% crc_bad={m['crc_bad']} rssi={m['rssi_dbm']} "
            f"sched_miss={m['scheduler_misses']} ready_p99={m['irq_to_rx_ready_us_p99']} "
            f"spi_to_rearm_p99={m['spi_end_to_rearm_start_us_p99']} polls={result['evidence']['progress_polls']}")
    if gate == "B":
        r, t = hop["rx"], hop["tx"]
        line += (f" | fp_match={hop['schedule_fp_match']} agree={r.get('agree')} disagree={r.get('disagree')} "
                 f"timeouts={r.get('timeout_advances')} resync={r.get('resync_entries')} locks={r.get('locks')} "
                 f"rx_retune_p99={r.get('retune_us_p99')} tx_retune_p99={t.get('retune_us_p99')} "
                 f"period_est={r.get('period_est_us')}")
    print(line, flush=True)
    write_summary(run_dir, result, hop)


def write_summary(run_dir: Path, result: dict, hop: dict) -> None:
    m, d = result["metrics"], result["derived"]
    rows = [f"# {result['run_id']}", "", f"- packets {result['packet_count']}, missing {m['missing']}, "
            f"loss {100 * d['loss_rate']:.3f} %, CRC good {m['crc_good']}, CRC bad {m['crc_bad']}, RSSI {m['rssi_dbm']}",
            f"- queue {m['queue_depth']}/{m['max_queue_depth']}, scheduler misses {m['scheduler_misses']}",
            f"- p99 µs: irq→spi {m['irq_to_spi_us_p99']}, spi {m['spi_duration_us_p99']}, rx_proc {m['rx_processing_us_p99']}, "
            f"spi_end→rearm {m['spi_end_to_rearm_start_us_p99']}, rearm {m['rx_rearm_us_p99']}, irq→ready {m['irq_to_rx_ready_us_p99']}",
            f"- progress polls {result['evidence'].get('progress_polls')}"]
    if hop:
        r, t = hop["rx"], hop["tx"]
        rows += [f"- schedule fingerprint TX==RX: {hop['schedule_fp_match']}, session {r.get('boot_session_id')}/"
                 f"{t.get('boot_session_id')}, map_version {r.get('map_version')}/{t.get('map_version')}",
                 f"- RX: agree {r.get('agree')}, disagree {r.get('disagree')}, timeout advances {r.get('timeout_advances')}, "
                 f"resync {r.get('resync_entries')}, locks {r.get('locks')}, max consecutive timeouts {r.get('max_consecutive_timeouts')}, "
                 f"period est {r.get('period_est_us')} µs",
                 f"- retune p99/max µs: RX {r.get('retune_us_p99')}/{r.get('retune_us_max')}, TX {t.get('retune_us_p99')}/{t.get('retune_us_max')}; "
                 f"hop compute p99 RX {r.get('hop_compute_us_p99')} TX {t.get('hop_compute_us_p99')}",
                 f"- TX logical next {t.get('logical')}, RX expected {r.get('logical')}"]
    (run_dir / "summary.md").write_text("\n".join(rows) + "\n", encoding="utf-8")


def cmd_summary() -> None:
    rows = []
    for res in sorted(HERE.glob("gate-*/*/result.json")):
        r = json.loads(res.read_text()); m = r["metrics"]; h = r.get("hop", {})
        rows.append({"gate": res.parent.parent.name[-1], "run_id": r["run_id"], "gap_us": r["gap_us"],
                     "packets": r["packet_count"], "missing": m["missing"], "loss_pct": round(100 * r["derived"]["loss_rate"], 4),
                     "crc_good": m["crc_good"], "crc_bad": m["crc_bad"], "rssi_dbm": m["rssi_dbm"],
                     "queue_depth": m["queue_depth"], "max_queue_depth": m["max_queue_depth"],
                     "scheduler_misses": m["scheduler_misses"], "irq_to_spi_us_p99": m["irq_to_spi_us_p99"],
                     "spi_duration_us_p99": m["spi_duration_us_p99"], "rx_processing_us_p99": m["rx_processing_us_p99"],
                     "spi_end_to_rearm_start_us_p99": m["spi_end_to_rearm_start_us_p99"], "rx_rearm_us_p99": m["rx_rearm_us_p99"],
                     "irq_to_rx_ready_us_p99": m["irq_to_rx_ready_us_p99"],
                     "afh_fp_match": h.get("schedule_fp_match"), "afh_agree": h.get("rx", {}).get("agree"),
                     "afh_disagree": h.get("rx", {}).get("disagree"), "afh_timeouts": h.get("rx", {}).get("timeout_advances"),
                     "afh_resync": h.get("rx", {}).get("resync_entries"), "afh_rx_retune_p99": h.get("rx", {}).get("retune_us_p99"),
                     "afh_tx_retune_p99": h.get("tx", {}).get("retune_us_p99"), "polls": r["evidence"].get("progress_polls")})
    (HERE / "matrix.json").write_text(json.dumps(rows, indent=2), encoding="utf-8")
    if rows:
        with (HERE / "matrix.csv").open("w", newline="", encoding="utf-8") as fh:
            w = csv.DictWriter(fh, fieldnames=list(rows[0])); w.writeheader(); w.writerows(rows)
    print(f"matrix rows: {len(rows)}")


if __name__ == "__main__":
    c = sys.argv[1]
    if c == "build":
        cmd_build()
    elif c == "run":
        cmd_run(sys.argv[2], int(sys.argv[3]), int(sys.argv[4]))
    elif c == "summary":
        cmd_summary()
