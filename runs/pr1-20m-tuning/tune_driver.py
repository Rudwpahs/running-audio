"""20 m link-budget tuning (leaves the frozen baseline: profile set by build flags).

  flash <rx|tx> [profile]     build + upload that role with the profile; TX boot is
                              captured and validated (role, profile, gap) -> tx_boot.log
  run   <target> [profile]    TX already back at the test spot on the power bank;
                              RX is reset, validated against the profile and captured.

profile flags (defaults = frozen baseline): --freq MHz --br kbps --cr {1,2,3} --dbm -18..3
Boards are found by USB serial (MAC), not by COM number.
"""
import argparse
import json
import os
import subprocess
import sys
import time
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPO / "tools"))

import pr1_experiment_controller as ctl  # noqa: E402
from pr1_board_sweep import (  # noqa: E402
    FROZEN_BASELINE, RX_FIELD_MAP, _parse_kv_and_telemetry, _validate_live_profile,
    build_run_metadata, classify_bottleneck,
)
from serial.tools import list_ports  # noqa: E402

MACS = {"rx": "E8:06:90:96:83:38", "tx": "B8:F8:62:D9:26:B4"}
GAP_US = 150
PIO = Path(sys.executable).with_name("pio.exe")
BUILD_ROOT = Path.home() / "pio-build"  # short path: long paths break PlatformIO on Windows
PROJECT = REPO / "firmware" / "t3s3_sx1280_runtime"
HERE = Path(__file__).resolve().parent


def find_port(role: str) -> str:
    for p in list_ports.comports():
        if (p.serial_number or "").upper() == MACS[role]:
            return p.device
    raise RuntimeError(f"{role} board ({MACS[role]}) not connected")


def profile_of(ns) -> dict:
    return {"frequency_mhz": float(ns.freq), "bitrate_kbps": ns.br,
            "coding_rate": ns.cr, "output_dbm": ns.dbm}


def tag_of(p: dict) -> str:
    return f"f{p['frequency_mhz']:g}-br{p['bitrate_kbps']}-cr{p['coding_rate']}-p{p['output_dbm']}"


def build_flags(p: dict) -> str:
    return (f"-D PR1_FREQ_KHZ={round(p['frequency_mhz'] * 1000)} "
            f"-D PR1_FLRC_BITRATE_KBPS={p['bitrate_kbps']} -D PR1_FLRC_CR={p['coding_rate']} "
            f"-D PR1_TX_OUTPUT_DBM={p['output_dbm']} -D PR1_TX_GAP_US={GAP_US}")


def stage_flash(role: str, p: dict) -> None:
    port = find_port(role)
    env = dict(os.environ, PLATFORMIO_BUILD_FLAGS=build_flags(p),
               PLATFORMIO_BUILD_DIR=str(BUILD_ROOT / f"{role}-{tag_of(p)}"))
    cmd = [str(PIO), "run", "--project-dir", str(PROJECT),
           "--environment", "rf_tx_compile" if role == "tx" else "rf_rx_compile",
           "--target", "upload", "--upload-port", port]
    if subprocess.run(cmd, env=env).returncode != 0:
        raise RuntimeError(f"{role} upload failed on {port}")

    # Boot block printed right after upload is lost before the port opens:
    # reset the new image with the port open and validate the fresh boot.
    run_dir = HERE / tag_of(p)
    run_dir.mkdir(parents=True, exist_ok=True)
    ser = ctl._open_serial(port)
    try:
        with (run_dir / f"{role}_boot.log").open("w", encoding="utf-8", buffering=1) as fh:
            ctl._drain_stale_input(ser)
            ctl._pulse_reset(ser)
            boot = ctl._read_until_marker(ser, fh, "PR1_RUNTIME_LIVE_READY", 10.0)
    finally:
        ser.close()
    if "PR1_RUNTIME_BOOT" not in (line.strip() for line in boot):
        raise TimeoutError(f"{role} LIVE_READY without fresh PR1_RUNTIME_BOOT")
    meta, _ = _parse_kv_and_telemetry(boot)
    _validate_live_profile(meta, role=role, expected_gap_us=GAP_US if role == "tx" else None,
                           source=role, profile=p)
    print(f"FLASH_OK role={role} port={port} profile={tag_of(p)}", flush=True)


def stage_run(target: int, p: dict, dist_m: float) -> None:
    run_dir = HERE / tag_of(p)
    run_dir.mkdir(parents=True, exist_ok=True)
    if not (run_dir / "tx_boot.log").exists():
        raise RuntimeError("flash tx with this profile first: no validated TX boot")
    for name in ("rx.log", "run_state.json", "result.json"):
        if (run_dir / name).exists():
            ctl.archive_partial_attempt(run_dir)
            break
    rx_port = find_port("rx")
    meta = build_run_metadata(GAP_US, target_packets=target, phase="revalidate",
                              firmware_sha=ctl.FROZEN_FIRMWARE_SHA,
                              run_id=f"tune-{dist_m:g}m-{tag_of(p)}-{target // 1000}k")
    meta.update({
        "gate": "gate5_distance_tuning", "distance_m": dist_m, "distance_source": "operator-reported",
        "profile": p, "profile_source": "build_flags_override_of_frozen_baseline",
        "frozen_baseline": FROZEN_BASELINE,
        "tx_power": "usb_power_bank_untethered", "ports": {"rx": rx_port, "tx": None},
        "build_identity": ctl.collect_build_identity(PROJECT, source_root=REPO),
    })
    (run_dir / "metadata.json").write_text(json.dumps(meta, indent=2), encoding="utf-8")
    ctl._write_state(run_dir, "running", run_id=meta["run_id"], started_utc=ctl._utc_now())

    rx = ctl._open_serial(rx_port)
    try:
        with (run_dir / "rx.log").open("w", encoding="utf-8", buffering=1) as fh:
            ctl._drain_stale_input(rx)
            ctl._pulse_reset(rx)
            boot = ctl._read_until_marker(rx, fh, "PR1_RUNTIME_LIVE_READY", 10.0)
            if "PR1_RUNTIME_BOOT" not in (line.strip() for line in boot):
                raise TimeoutError("rx LIVE_READY without fresh PR1_RUNTIME_BOOT")
            rx_meta, _ = _parse_kv_and_telemetry(boot)
            # output_dbm only acts on transmit: the RX image may carry any value.
            rx_profile = {**p, "output_dbm": int(rx_meta.get("output_dbm", p["output_dbm"]))}
            _validate_live_profile(rx_meta, role="rx", expected_gap_us=None, source="rx", profile=rx_profile)

            time.sleep(ctl.estimate_initial_wait_s(GAP_US, target))
            observed, polls = 0, 0
            while observed < target:
                if polls >= 600:
                    raise TimeoutError(f"target not reached: {observed}/{target}")
                lines = ctl._snapshot_with_recovery(rx, fh, 1.0)
                polls += 1
                try:
                    observed = ctl._packet_span_from_snapshot(lines)
                except ValueError:
                    pass
                if observed < target:
                    time.sleep(0.75)
            time.sleep(max(0.25, ctl.estimate_initial_wait_s(GAP_US, 128)))
            ctl._snapshot_with_recovery(rx, fh, 1.0)
    except Exception as exc:
        ctl._write_state(run_dir, "error", run_id=meta["run_id"], error=str(exc))
        raise
    finally:
        rx.close()

    log = (run_dir / "rx.log").read_text(encoding="utf-8", errors="replace").splitlines()
    _, tel = _parse_kv_and_telemetry(log)
    metrics = {out: tel.get(raw) for raw, out in RX_FIELD_MAP.items()}
    metrics["scheduler_misses"] = None
    good, missing, bad = metrics["crc_good"], metrics["missing"], metrics["crc_bad"]
    count = good + missing
    loss = missing / count if count else 0.0
    first = first_snapshot(log)
    result = {
        "run_id": meta["run_id"], "phase": "gate5_distance_tuning", "gap_us": GAP_US,
        "distance_m": dist_m, "profile": p, "target_packets": target, "packet_count": count,
        "metrics": metrics,
        "derived": {"loss_rate": loss, "crc_bad_rate": (bad / count) if count else 0.0,
                    "unpolled_window_loss_rate": first, "target_reached": count >= target},
        "classification": classify_bottleneck(metrics, loss),
        "evidence": {
            "packet_count_source": "rx_sequence_span=crc_good+missing",
            "unpolled_window": "first telemetry snapshot, before any host progress poll",
            "scheduler_misses": "unavailable: TX untethered on power bank; TX boot validated at flash (tx_boot.log)",
            "evidence_boundary": "measured_diagnostics_not_causal_proof",
        },
    }
    (run_dir / "result.json").write_text(json.dumps(result, indent=2), encoding="utf-8")
    ctl._write_state(run_dir, "complete", run_id=meta["run_id"], packet_count=count, finished_utc=ctl._utc_now())
    print(f"RUN {tag_of(p)} packets={count} missing={missing} loss={100*loss:.3f}% "
          f"unpolled={100*(first or 0):.2f}% crc_bad={bad} rssi={metrics['rssi_dbm']}", flush=True)


def first_snapshot(lines: list[str]) -> float | None:
    """Loss in the first telemetry snapshot (no host poll has happened yet)."""
    good = None
    for line in lines:
        if not line.startswith("PR1T "):
            continue
        kv = dict(part.split("=", 1) for part in line.split()[1:] if "=" in part)
        if kv.get("field") == "crc_good":
            good = int(kv["value"])
        elif kv.get("field") == "missing" and good is not None:
            missing = int(kv["value"])
            return missing / (good + missing) if good + missing else None
    return None


if __name__ == "__main__":
    ap = argparse.ArgumentParser()
    ap.add_argument("stage", choices=["flash", "run"])
    ap.add_argument("arg", help="flash: rx|tx; run: target packets")
    ap.add_argument("--freq", type=float, default=FROZEN_BASELINE["frequency_mhz"])
    ap.add_argument("--br", type=int, default=FROZEN_BASELINE["bitrate_kbps"])
    ap.add_argument("--cr", type=int, default=FROZEN_BASELINE["coding_rate"])
    ap.add_argument("--dbm", type=int, default=FROZEN_BASELINE["output_dbm"])
    ap.add_argument("--dist", type=float, default=20.0)
    ns = ap.parse_args()
    prof = profile_of(ns)
    if ns.stage == "flash":
        stage_flash(ns.arg, prof)
    else:
        stage_run(int(ns.arg), prof, ns.dist)
