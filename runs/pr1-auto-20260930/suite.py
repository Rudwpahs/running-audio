"""Unattended two-board suite, 2026-09-30 (both boards on the host, bench spacing unchanged).

Host orchestration only. Images come from this branch: frozen RF defaults + boot-only
OLED role label; A/B images set PR1_FLRC_CR via build flags (tuning branch #51).

  build             build every image this suite needs (parallel)
  step1             150 us 10k on the OLED image  -> compare with 2026-09-29 (0.30 %)
  step2             fine sweep 150/145/140/135/130/125 us, 10k each, same session
  step4             CR 1/2 A/B: gaps 200/175/150/125 us, 10k each (A/B, not baseline)
  restore           flash both boards back to the baseline OLED images, validate boot
  step3 <hours>     150 us long run, one snapshot per minute
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
from pr1_board_sweep import (  # noqa: E402
    FROZEN_BASELINE, _parse_kv_and_telemetry, _validate_live_profile, parse_run_logs,
)
from serial.tools import list_ports  # noqa: E402

HERE = Path(__file__).resolve().parent
PROJECT = REPO / "firmware" / "t3s3_sx1280_runtime"
PIO = str(Path(os.environ["USERPROFILE"]) / ".platformio" / "penv" / "Scripts" / "pio.exe")
PY = str(Path(os.environ["USERPROFILE"]) / ".platformio" / "penv" / "Scripts" / "python.exe")
BOOT_APP0 = Path(os.environ["USERPROFILE"]) / ".platformio/packages/framework-arduinoespressif32/tools/partitions/boot_app0.bin"
MAC = {"rx": "E8:06:90:96:83:38", "tx": "B8:F8:62:D9:26:B4"}
TARGET = 10_000

STEP2_GAPS = [150, 145, 140, 135, 130, 125]
STEP4_GAPS = [200, 175, 150, 125]
CR12 = {"coding_rate": 2}


def image_name(role: str, gap: int | None, cr: int) -> str:
    return f"{role}-cr{cr}" + (f"-{gap}us" if role == "tx" else "")


def needed_images() -> list[tuple[str, int | None, int]]:
    imgs = [("rx", None, 3), ("rx", None, 2)]
    imgs += [("tx", g, 3) for g in STEP2_GAPS]
    imgs += [("tx", g, 2) for g in STEP4_GAPS]
    return imgs


def render_ini(role: str, gap: int | None, cr: int) -> Path:
    text = (PROJECT / "platformio.ini").read_text(encoding="utf-8")
    if role == "tx":
        text = text.replace("-D PR1_TX_GAP_US=5000", f"-D PR1_TX_GAP_US={gap}\n    -D PR1_FLRC_CR={cr}", 1)
    else:
        text = text.replace("-D PR1_RUNTIME_ROLE=2", f"-D PR1_RUNTIME_ROLE=2\n    -D PR1_FLRC_CR={cr}", 1)
    path = HERE / "generated" / f"{image_name(role, gap, cr)}.ini"
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text, encoding="utf-8")
    return path


def build_dir(role: str, gap: int | None, cr: int) -> Path:
    env = "rf_tx_compile" if role == "tx" else "rf_rx_compile"
    return HERE / "build" / image_name(role, gap, cr) / env


def build_one(img) -> tuple[str, int]:
    role, gap, cr = img
    ini = render_ini(role, gap, cr)
    env = dict(os.environ, PLATFORMIO_BUILD_DIR=str(build_dir(role, gap, cr).parent))
    log = HERE / "build" / f"{image_name(role, gap, cr)}.log"
    log.parent.mkdir(parents=True, exist_ok=True)
    with log.open("w", encoding="utf-8") as fh:
        rc = subprocess.run([PIO, "run", "-d", str(PROJECT), "-c", str(ini), "-e",
                             "rf_tx_compile" if role == "tx" else "rf_rx_compile"],
                            env=env, stdout=fh, stderr=subprocess.STDOUT).returncode
    return image_name(role, gap, cr), rc


def cmd_build() -> None:
    with ThreadPoolExecutor(max_workers=3) as pool:
        for name, rc in pool.map(build_one, needed_images()):
            print(f"BUILD {name} rc={rc}", flush=True)
            if rc != 0:
                raise SystemExit(f"build failed: {name}")


def port_of(role: str) -> str:
    for p in list_ports.comports():
        if MAC[role] in (p.hwid or "").upper():
            return p.device
    raise RuntimeError(f"{role} board ({MAC[role]}) not connected")


def flash(role: str, gap: int | None, cr: int) -> None:
    b = build_dir(role, gap, cr)
    fw = b / "firmware.bin"
    out = subprocess.run([PY, "-m", "esptool", "--chip", "esp32s3", "--port", port_of(role), "--baud", "921600",
                          "--before", "default_reset", "--after", "hard_reset", "write_flash", "-z",
                          "--flash_mode", "dio", "--flash_freq", "80m", "--flash_size", "4MB",
                          "0x0", str(b / "bootloader.bin"), "0x8000", str(b / "partitions.bin"),
                          "0xe000", str(BOOT_APP0), "0x10000", str(fw)],
                         capture_output=True, text=True)
    if out.returncode != 0 or out.stdout.count("Hash of data verified") != 4 or MAC[role].lower() not in out.stdout.lower():
        raise RuntimeError(f"flash failed for {image_name(role, gap, cr)}: {out.stdout[-400:]}{out.stderr[-400:]}")
    print(f"FLASH {image_name(role, gap, cr)} -> {role} sha256 {ctl._sha256(fw)[:12]}", flush=True)


def run_capture(run_id: str, gap: int, target: int, profile: dict | None, phase_note: str) -> dict:
    run_dir = HERE / "runs" / run_id
    run_dir.mkdir(parents=True, exist_ok=True)
    result_path = run_dir / "result.json"
    if result_path.exists() and json.loads(result_path.read_text())["derived"]["target_reached"]:
        print(f"SKIP {run_id} (already complete)", flush=True)
        return json.loads(result_path.read_text())
    if (run_dir / "rx.log").exists():
        ctl.archive_partial_attempt(run_dir)
    meta = {
        "schema_version": 1, "run_id": run_id, "created_utc": ctl._utc_now(), "gap_us": gap,
        "target_packets": target, "phase": "revalidate", "firmware_sha": ctl.FROZEN_FIRMWARE_SHA,
        "frozen_baseline": dict(FROZEN_BASELINE), "expected_profile_override": profile,
        "files": {"rx_log": "rx.log", "tx_log": "tx.log"}, "notes": phase_note,
        "image": "frozen RF defaults + boot-only OLED role label" + (f" + {profile}" if profile else ""),
        "setup": "both boards on host PC, bench spacing unchanged from 2026-09-30 OLED check (RSSI ~-40)",
        "build_identity": ctl.collect_build_identity(PROJECT, source_root=REPO),
    }
    (run_dir / "metadata.json").write_text(json.dumps(meta, indent=2), encoding="utf-8")
    period_s = (gap + 2750) / 1e6
    rx = ctl._open_serial(port_of("rx"))
    tx = ctl._open_serial(port_of("tx"))
    try:
        ctl._drain_stale_input(tx)
        ctl._pulse_reset(tx)
        ctl._drain_stale_input(rx)
        ctl._pulse_reset(rx)
        result = ctl.capture_run_from_serial(meta, run_dir, rx, tx, initial_wait_s=0.7 * target * period_s,
                                             settle_s=0.5, max_polls=100, profile=profile)
    finally:
        tx.close()
        rx.close()
    m = result["metrics"]
    print(f"RUN {run_id} packets={result['packet_count']} missing={m['missing']} "
          f"loss={100 * result['derived']['loss_rate']:.3f}% crc_bad={m['crc_bad']} rssi={m['rssi_dbm']} "
          f"irq_to_ready_p99={m['irq_to_rx_ready_us_p99']} polls={result['evidence']['progress_polls']}", flush=True)
    return result


OLED_BRANCH_BUILD = {  # images from claude/pr1-oled-role-label 1a70f95 (used in step1)
    "rx": REPO / "runs" / "pr1-oled-label" / "build" / "rx" / "rf_rx_compile",
    "tx": REPO / "runs" / "pr1-oled-label" / "build" / "tx150" / "rf_tx_compile",
}


def flash_dir(role: str, b: Path, label: str) -> None:
    fw = b / "firmware.bin"
    out = subprocess.run([PY, "-m", "esptool", "--chip", "esp32s3", "--port", port_of(role), "--baud", "921600",
                          "--before", "default_reset", "--after", "hard_reset", "write_flash", "-z",
                          "--flash_mode", "dio", "--flash_freq", "80m", "--flash_size", "4MB",
                          "0x0", str(b / "bootloader.bin"), "0x8000", str(b / "partitions.bin"),
                          "0xe000", str(BOOT_APP0), "0x10000", str(fw)], capture_output=True, text=True)
    if out.returncode != 0 or out.stdout.count("Hash of data verified") != 4:
        raise RuntimeError(f"flash failed for {label}")
    print(f"FLASH {label} -> {role} sha256 {ctl._sha256(fw)[:12]}", flush=True)


def cmd_imgab() -> None:
    """A/B/A: OLED-branch image vs suite-branch image at 150 us, same bench."""
    for i, which in enumerate(["oled", "suite", "oled"]):
        for role in ("rx", "tx"):
            if which == "oled":
                flash_dir(role, OLED_BRANCH_BUILD[role], f"oled-branch-{role}")
            else:
                flash(role, 150 if role == "tx" else None, 3)
        run_capture(f"ab{i}-{which}-gap-150us-10k", 150, TARGET, None,
                    f"image A/B/A at 150 us: {which} image")


def ensure(role: str, gap: int | None, cr: int, state: dict) -> None:
    key = image_name(role, gap, cr)
    if state.get(role) != key:
        flash(role, gap, cr)
        state[role] = key


def cmd_steps(which: str) -> None:
    state: dict = {}
    if which == "step1":
        if os.environ.get("PR1_NOFLASH"):
            # Boards already hold the OLED-branch images flashed and boot-checked earlier today.
            state = {"rx": image_name("rx", None, 3), "tx": image_name("tx", 150, 3)}
            print("STEP1 using images already on the boards (claude/pr1-oled-role-label 1a70f95)", flush=True)
        ensure("rx", None, 3, state); ensure("tx", 150, 3, state)
        run_capture("s1-gap-150us-10k-oled", 150, TARGET, None, "step1: OLED image vs 2026-09-29 150us 10k")
    elif which == "step2":
        ensure("rx", None, 3, state)
        for gap in STEP2_GAPS:
            ensure("tx", gap, 3, state)
            run_capture(f"s2-gap-{gap}us-10k", gap, TARGET, None, "step2: fine sweep 125..150 us")
    elif which == "step2r":
        # Control: repeat 150 us first, then the sweep in reverse order to separate gap from time.
        ensure("rx", None, 3, state)
        for i, gap in enumerate([150] + STEP2_GAPS[::-1]):
            ensure("tx", gap, 3, state)
            run_capture(f"s2r{i}-gap-{gap}us-10k", gap, TARGET, None,
                        "step2 control: 150 us repeat, then reverse sweep 125..150 us")
    elif which == "step4":
        ensure("rx", None, 2, state)
        for gap in STEP4_GAPS:
            ensure("tx", gap, 2, state)
            run_capture(f"s4-cr12-gap-{gap}us-10k", gap, TARGET, CR12, "step4: CR 1/2 A/B (not baseline)")
        # Same-session CR 3/4 control at 150 us to catch environment drift during step4.
        ensure("rx", None, 3, state); ensure("tx", 150, 3, state)
        run_capture("s4-ctrl-cr34-gap-150us-10k", 150, TARGET, None, "step4 control: CR 3/4 150 us after CR 1/2 runs")
    elif which == "step4r":
        # Interleaved repeats: CR 1/2 vs CR 3/4 at 150 us, then CR 1/2 175 us again.
        plan = [(2, 150), (3, 150), (2, 150), (3, 150), (2, 175)]
        for i, (cr, gap) in enumerate(plan):
            ensure("rx", None, cr, state); ensure("tx", gap, cr, state)
            run_capture(f"s4r{i}-cr{'12' if cr == 2 else '34'}-gap-{gap}us-10k", gap, TARGET,
                        CR12 if cr == 2 else None, "step4 interleaved repeat (A/B, not baseline)")
    elif which == "restore":
        ensure("rx", None, 3, state); ensure("tx", 150, 3, state)
        for role in ("rx", "tx"):
            ser = ctl._open_serial(port_of(role))
            try:
                with (HERE / f"restore_{role}_boot.log").open("w", encoding="utf-8") as fh:
                    ctl._drain_stale_input(ser); ctl._pulse_reset(ser)
                    boot = ctl._read_until_marker(ser, fh, "PR1_RUNTIME_LIVE_READY", 10.0)
            finally:
                ser.close()
            meta, _ = _parse_kv_and_telemetry(boot)
            _validate_live_profile(meta, role=role, expected_gap_us=150 if role == "tx" else None, source=role)
            print(f"RESTORE {role} ok oled={meta.get('oled_role_label')}", flush=True)


def snapshot_values(lines: list[str]) -> dict:
    _, tel = _parse_kv_and_telemetry(lines)
    return tel


def cmd_step3(hours: float) -> None:
    run_id = f"s3-gap-150us-longrun-{hours:g}h"
    run_dir = HERE / "runs" / run_id
    run_dir.mkdir(parents=True, exist_ok=True)
    if (run_dir / "rx.log").exists():
        ctl.archive_partial_attempt(run_dir)
    meta = {"schema_version": 1, "run_id": run_id, "created_utc": ctl._utc_now(), "gap_us": 150,
            "target_packets": 1, "phase": "revalidate", "firmware_sha": ctl.FROZEN_FIRMWARE_SHA,
            "duration_h": hours, "notes": "step3: long-run stability, one RX snapshot per 60 s",
            "image": "frozen RF defaults + boot-only OLED role label",
            "build_identity": ctl.collect_build_identity(PROJECT, source_root=REPO)}
    (run_dir / "metadata.json").write_text(json.dumps(meta, indent=2), encoding="utf-8")
    ctl._write_state(run_dir, "running", run_id=run_id, started_utc=ctl._utc_now())
    rx = ctl._open_serial(port_of("rx"))
    tx = ctl._open_serial(port_of("tx"))
    series = run_dir / "timeseries.csv"
    try:
        with (run_dir / "rx.log").open("w", encoding="utf-8", buffering=1) as rx_fh, \
                (run_dir / "tx.log").open("w", encoding="utf-8", buffering=1) as tx_fh, \
                series.open("w", newline="", encoding="utf-8") as sfh:
            ctl._drain_stale_input(tx); ctl._pulse_reset(tx)
            tx_boot = ctl._read_until_marker(tx, tx_fh, "PR1_RUNTIME_LIVE_READY", 10.0)
            ctl._drain_stale_input(rx); ctl._pulse_reset(rx)
            rx_boot = ctl._read_until_marker(rx, rx_fh, "PR1_RUNTIME_LIVE_READY", 10.0)
            _validate_live_profile(_parse_kv_and_telemetry(tx_boot)[0], role="tx", expected_gap_us=150, source="tx")
            _validate_live_profile(_parse_kv_and_telemetry(rx_boot)[0], role="rx", expected_gap_us=None, source="rx")
            writer = csv.writer(sfh)
            cols = ["elapsed_s", "crc_good", "missing", "crc_bad", "rssi_dbm", "max_queue_depth",
                    "irq_to_spi_us", "spi_duration_us", "spi_end_to_rearm_start_us", "rx_rearm_us",
                    "irq_to_rx_ready_us", "rx_t_us", "reopens"]
            writer.writerow(cols)
            start = time.monotonic()
            deadline = start + hours * 3600
            next_at = start + 60
            while True:
                time.sleep(max(0.0, next_at - time.monotonic()))
                lines = ctl._snapshot_with_recovery(rx, rx_fh, 1.0)
                tel = snapshot_values(lines)
                t_us = next((int(tok[5:]) for ln in lines for tok in ln.split() if tok.startswith("t_us=")), None)
                reopens = (run_dir / "rx.log").read_text(encoding="utf-8", errors="replace").count("PR1_HOST_SERIAL_REOPEN")
                writer.writerow([round(time.monotonic() - start, 1)] + [tel.get(c) for c in cols[1:11]] + [t_us, reopens])
                sfh.flush()
                if tel.get("crc_good") is not None:
                    span = tel["crc_good"] + tel.get("missing", 0)
                    print(f"TICK {int(time.monotonic() - start)}s span={span} missing={tel.get('missing')} "
                          f"crc_bad={tel.get('crc_bad')} rssi={tel.get('rssi_dbm')} "
                          f"ready_p99={tel.get('irq_to_rx_ready_us')} reopens={reopens}", flush=True)
                if time.monotonic() >= deadline:
                    break
                next_at += 60
            ctl._snapshot_with_recovery(tx, tx_fh, 1.0)
    except Exception as exc:
        ctl._write_state(run_dir, "error", run_id=run_id, error=str(exc))
        raise
    finally:
        tx.close(); rx.close()
    result = parse_run_logs(meta, (run_dir / "rx.log").read_text(encoding="utf-8", errors="replace").splitlines(),
                            (run_dir / "tx.log").read_text(encoding="utf-8", errors="replace").splitlines())
    (run_dir / "result.json").write_text(json.dumps(result, indent=2), encoding="utf-8")
    ctl._write_state(run_dir, "complete", run_id=run_id, packet_count=result["packet_count"], finished_utc=ctl._utc_now())
    m = result["metrics"]
    print(f"LONGRUN done packets={result['packet_count']} missing={m['missing']} "
          f"loss={100 * result['derived']['loss_rate']:.4f}% crc_bad={m['crc_bad']} sched_miss={m['scheduler_misses']}", flush=True)


if __name__ == "__main__":
    cmd = sys.argv[1]
    if cmd == "build":
        cmd_build()
    elif cmd == "imgab":
        cmd_imgab()
    elif cmd == "step3":
        cmd_step3(float(sys.argv[2]))
    else:
        cmd_steps(cmd)
