#!/usr/bin/env python3
"""Host controller for the frozen PR1 two-board SX1280 sweep.

This module deliberately does not modify firmware or RF/driver behavior.  It
coordinates generated PlatformIO configs, cached builds, serial capture,
partial-result preservation, and the existing board-sweep parser/analyzer.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import shutil
import subprocess
import sys
import time
from datetime import datetime, timezone
from pathlib import Path
from typing import Callable, Iterable

from pr1_board_sweep import (
    SWEEP_GAPS_US,
    _parse_kv_and_telemetry,
    _validate_live_profile,
    analyze_directory,
    create_plan,
    experiment_status,
    parse_run_logs,
    render_tx_sweep_config,
)

FROZEN_FIRMWARE_SHA = "d1b7ec2b1130fd63fb0eb11fd900b0622766f14c"
SERIAL_BAUD = 115200
CONTROLLER_SCHEMA_VERSION = 1


def _utc_now() -> str:
    return datetime.now(timezone.utc).isoformat().replace("+00:00", "Z")


def _stamp_now() -> str:
    return datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ")


def _sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as fh:
        for chunk in iter(lambda: fh.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def _command_output(command: list[str], *, cwd: Path | None = None) -> str:
    try:
        completed = subprocess.run(
            command,
            cwd=str(cwd) if cwd else None,
            capture_output=True,
            text=True,
            check=False,
        )
    except OSError as exc:
        return f"unavailable: {exc}"
    if completed.returncode != 0:
        text = (completed.stderr or completed.stdout).strip()
        return f"unavailable: {text or f'exit {completed.returncode}'}"
    return completed.stdout.strip()


def collect_build_identity(project_dir: Path, *, source_root: Path | None = None) -> dict:
    source_root = source_root or project_dir.resolve().parents[1]
    platformio_ini = project_dir / "platformio.ini"
    head = _command_output(["git", "rev-parse", "HEAD"], cwd=source_root)
    dirty_output = _command_output(
        ["git", "status", "--porcelain", "--untracked-files=no"], cwd=source_root
    )
    dirty = bool(dirty_output) and not dirty_output.startswith("unavailable:")
    return {
        "source_head_sha": head,
        "frozen_firmware_sha": FROZEN_FIRMWARE_SHA,
        "git_dirty": dirty,
        "platformio_version": _command_output(["pio", "--version"]),
        "platformio_ini_sha256": _sha256(platformio_ini),
        "controller_file_sha256": _sha256(Path(__file__)),
    }


def build_session_metadata(
    *,
    firmware_sha: str,
    target_packets: int,
    rx_port: str,
    tx_port: str,
    identity: dict,
    created_utc: str | None = None,
) -> dict:
    if target_packets <= 0:
        raise ValueError("target_packets must be positive")
    return {
        "schema_version": CONTROLLER_SCHEMA_VERSION,
        "created_utc": created_utc or _utc_now(),
        "firmware_sha": firmware_sha,
        "target_packets": target_packets,
        "sweep_gaps_us": list(SWEEP_GAPS_US),
        "ports": {"rx": rx_port, "tx": tx_port},
        "serial_baud": SERIAL_BAUD,
        "build_identity": dict(identity),
        "evidence_boundary": "host_orchestration_only_frozen_rf_baseline_unchanged",
    }


def _build_entry(
    *,
    project_dir: Path,
    environment: str,
    build_dir: Path,
    config_path: Path | None = None,
    gap_us: int | None = None,
) -> dict:
    command = ["pio", "run", "--project-dir", str(project_dir)]
    if config_path is not None:
        command += ["--project-conf", str(config_path)]
    command += ["--environment", environment]
    return {
        "environment": environment,
        "gap_us": gap_us,
        "config_path": str(config_path) if config_path else None,
        "build_dir": str(build_dir),
        "build_command": command,
        "env": {"PLATFORMIO_BUILD_DIR": str(build_dir)},
    }


def prepare_build_matrix(session_root: Path, project_dir: Path) -> dict:
    """Generate frozen per-gap configs and independent PlatformIO build caches."""
    # PlatformIO chdirs into --project-dir before resolving --project-conf and
    # PLATFORMIO_BUILD_DIR, so relative paths would land inside the firmware tree.
    session_root = session_root.resolve()
    project_dir = project_dir.resolve()
    base_path = project_dir / "platformio.ini"
    base_text = base_path.read_text(encoding="utf-8")
    generated_dir = session_root / "generated"
    build_root = session_root / "build"
    generated_dir.mkdir(parents=True, exist_ok=True)
    build_root.mkdir(parents=True, exist_ok=True)

    safe = _build_entry(
        project_dir=project_dir,
        environment="safe",
        build_dir=build_root / "safe",
    )
    rx = _build_entry(
        project_dir=project_dir,
        environment="rf_rx_compile",
        build_dir=build_root / "rx",
    )
    tx: list[dict] = []
    for gap_us in SWEEP_GAPS_US:
        config_path = generated_dir / f"tx-{gap_us}us.ini"
        config_path.write_text(render_tx_sweep_config(base_text, gap_us), encoding="utf-8")
        tx.append(
            _build_entry(
                project_dir=project_dir,
                environment="rf_tx_compile",
                build_dir=build_root / f"tx-{gap_us}us",
                config_path=config_path,
                gap_us=gap_us,
            )
        )

    matrix = {"safe": safe, "rx": rx, "tx": tx}
    (session_root / "build_matrix.json").write_text(
        json.dumps(matrix, indent=2), encoding="utf-8"
    )
    return matrix


def _upload_command(entry: dict, port: str) -> list[str]:
    return list(entry["build_command"]) + ["--target", "upload", "--upload-port", port]


def build_dry_run_plan(
    *,
    project_dir: Path,
    session_root: Path,
    rx_port: str,
    tx_port: str,
    include_safe: bool,
) -> dict:
    """Describe the minimum-flash physical sequence without touching hardware."""
    steps: list[dict] = []
    if include_safe:
        steps.extend(
            [
                {"kind": "safe_flash", "board": "rx", "port": rx_port},
                {"kind": "safe_flash", "board": "tx", "port": tx_port},
            ]
        )
    steps.append({"kind": "rx_flash", "board": "rx", "port": rx_port})
    for gap_us in SWEEP_GAPS_US:
        steps.append(
            {"kind": "tx_gap_flash", "board": "tx", "port": tx_port, "gap_us": gap_us}
        )
        # After the TX flash so no previous-gap packet reaches the fresh RX.
        steps.append(
            {
                "kind": "rx_reset",
                "board": "rx",
                "port": rx_port,
                "verification": "fresh PR1_RUNTIME_BOOT + PR1_RUNTIME_LIVE_READY required",
            }
        )
        steps.append(
            {
                "kind": "capture",
                "gap_us": gap_us,
                "target_packets": 1000,
                "partial_results": "preserved on interrupt/error",
            }
        )
    steps.append({"kind": "analyze", "session_root": str(session_root)})
    return {
        "project_dir": str(project_dir),
        "session_root": str(session_root),
        "frozen_firmware_sha": FROZEN_FIRMWARE_SHA,
        "steps": steps,
    }


def estimate_initial_wait_s(gap_us: int, target_packets: int) -> float:
    """Conservative host polling delay; this is not an RF throughput claim."""
    if gap_us not in SWEEP_GAPS_US:
        raise ValueError(f"unsupported sweep gap_us: {gap_us}")
    if target_packets <= 0:
        raise ValueError("target_packets must be positive")
    # Blocking TX airtime/host overhead is intentionally represented by a
    # conservative fixed allowance rather than inferred from measured RF data.
    nominal_packet_cycle_us = gap_us + 2500
    return max(0.5, target_packets * nominal_packet_cycle_us / 1_000_000.0 * 0.70)


def _write_state(run_dir: Path, status: str, **extra) -> None:
    state = {"status": status, "updated_utc": _utc_now(), **extra}
    (run_dir / "run_state.json").write_text(json.dumps(state, indent=2), encoding="utf-8")


def _decode_line(raw: bytes | str) -> str:
    if isinstance(raw, bytes):
        return raw.decode("utf-8", errors="replace").rstrip("\r\n")
    return str(raw).rstrip("\r\n")


SNAPSHOT_LAST_FIELD = "field=capability_mask"
SNAPSHOT_IDLE_READS = 3


def _read_complete_line(serial_obj, pending: bytearray) -> str | None:
    """Return one newline-terminated line, or None when nothing complete yet.

    pyserial's readline() returns a partial line on timeout; USB delivery can
    split a line, so fragments are joined until the newline arrives.
    """
    raw = serial_obj.readline()
    if not raw:
        return None
    pending.extend(raw.encode() if isinstance(raw, str) else raw)
    if not pending.endswith(b"\n"):
        return None
    line = _decode_line(bytes(pending))
    pending.clear()
    return line


def _read_until_marker(serial_obj, log_fh, marker: str, timeout_s: float) -> list[str]:
    deadline = time.monotonic() + timeout_s
    lines: list[str] = []
    pending = bytearray()
    while time.monotonic() < deadline:
        line = _read_complete_line(serial_obj, pending)
        if line is None:
            continue
        log_fh.write(line + "\n")
        log_fh.flush()
        lines.append(line)
        if line.strip() == marker:
            return lines
    raise TimeoutError(f"serial marker not observed: {marker}")


def _request_snapshot(serial_obj, log_fh, timeout_s: float) -> list[str]:
    serial_obj.write(b"t")
    if hasattr(serial_obj, "flush"):
        serial_obj.flush()
    deadline = time.monotonic() + timeout_s
    lines: list[str] = []
    pending = bytearray()
    idle_reads = 0
    while time.monotonic() < deadline:
        line = _read_complete_line(serial_obj, pending)
        if line is None:
            # Stop only after the snapshot went quiet with no fragment pending.
            idle_reads = 0 if pending else idle_reads + 1
            if lines and not pending and idle_reads >= SNAPSHOT_IDLE_READS:
                break
            continue
        idle_reads = 0
        log_fh.write(line + "\n")
        log_fh.flush()
        lines.append(line)
        if SNAPSHOT_LAST_FIELD in line:
            break
    if pending:
        # Keep the evidence, but never parse a truncated line as telemetry.
        log_fh.write("PR1_HOST_TRUNCATED_LINE " + _decode_line(bytes(pending)) + "\n")
        log_fh.flush()
    if not lines:
        raise TimeoutError("telemetry snapshot produced no serial lines")
    return lines


def _packet_span_from_snapshot(lines: Iterable[str]) -> int:
    _, telemetry = _parse_kv_and_telemetry(lines)
    good = telemetry.get("crc_good")
    missing = telemetry.get("missing")
    if not isinstance(good, int) or not isinstance(missing, int):
        raise ValueError("progress snapshot missing crc_good or missing")
    return good + missing


def capture_run_from_serial(
    metadata: dict,
    run_dir: Path,
    rx_serial,
    tx_serial,
    *,
    sleep_fn: Callable[[float], None] = time.sleep,
    initial_wait_s: float | None = None,
    poll_wait_s: float = 0.75,
    settle_s: float | None = None,
    read_timeout_s: float = 1.0,
    boot_timeout_s: float = 10.0,
    max_polls: int = 240,
) -> dict:
    """Capture one run while continuously preserving serial evidence to disk."""
    run_dir.mkdir(parents=True, exist_ok=True)
    gap_us = int(metadata["gap_us"])
    target_packets = int(metadata["target_packets"])
    wait_s = estimate_initial_wait_s(gap_us, target_packets) if initial_wait_s is None else initial_wait_s
    final_settle_s = max(0.25, estimate_initial_wait_s(gap_us, 128)) if settle_s is None else settle_s

    _write_state(
        run_dir,
        "running",
        run_id=metadata["run_id"],
        gap_us=gap_us,
        target_packets=target_packets,
        started_utc=_utc_now(),
    )

    rx_path = run_dir / "rx.log"
    tx_path = run_dir / "tx.log"
    try:
        with rx_path.open("w", encoding="utf-8", buffering=1) as rx_fh, tx_path.open(
            "w", encoding="utf-8", buffering=1
        ) as tx_fh:
            rx_boot = _read_until_marker(rx_serial, rx_fh, "PR1_RUNTIME_LIVE_READY", boot_timeout_s)
            tx_boot = _read_until_marker(tx_serial, tx_fh, "PR1_RUNTIME_LIVE_READY", boot_timeout_s)
            for name, boot in (("rx", rx_boot), ("tx", tx_boot)):
                if "PR1_RUNTIME_BOOT" not in (line.strip() for line in boot):
                    raise TimeoutError(f"{name} LIVE_READY observed without a fresh PR1_RUNTIME_BOOT")
            rx_meta, _ = _parse_kv_and_telemetry(rx_boot)
            tx_meta, _ = _parse_kv_and_telemetry(tx_boot)
            _validate_live_profile(rx_meta, role="rx", expected_gap_us=None, source="rx")
            _validate_live_profile(tx_meta, role="tx", expected_gap_us=gap_us, source="tx")

            sleep_fn(wait_s)
            observed = 0
            polls = 0
            while observed < target_packets:
                if polls >= max_polls:
                    raise TimeoutError(
                        f"packet target not reached after {max_polls} telemetry polls: {observed}/{target_packets}"
                    )
                progress_lines = _request_snapshot(rx_serial, rx_fh, read_timeout_s)
                polls += 1
                try:
                    observed = _packet_span_from_snapshot(progress_lines)
                except ValueError:
                    # An incomplete progress snapshot only delays the stop
                    # decision; the final snapshot is still validated strictly.
                    sleep_fn(poll_wait_s)
                    continue
                if observed < target_packets:
                    sleep_fn(poll_wait_s)

            # A progress telemetry request can perturb the exact timing path.
            # Allow a clean no-telemetry interval, then take one authoritative
            # final RX snapshot. The firmware's DurationWindow<64> p99 values
            # remain the source of timing percentiles.
            sleep_fn(final_settle_s)
            _request_snapshot(rx_serial, rx_fh, read_timeout_s)
            _request_snapshot(tx_serial, tx_fh, read_timeout_s)

        result = parse_run_logs(
            metadata,
            rx_path.read_text(encoding="utf-8", errors="replace").splitlines(),
            tx_path.read_text(encoding="utf-8", errors="replace").splitlines(),
        )
        (run_dir / "result.json").write_text(json.dumps(result, indent=2), encoding="utf-8")
        _write_state(
            run_dir,
            "complete",
            run_id=metadata["run_id"],
            gap_us=gap_us,
            target_packets=target_packets,
            packet_count=result["packet_count"],
            finished_utc=_utc_now(),
        )
        return result
    except KeyboardInterrupt:
        _write_state(
            run_dir,
            "interrupted",
            run_id=metadata["run_id"],
            gap_us=gap_us,
            target_packets=target_packets,
        )
        raise
    except Exception as exc:
        _write_state(
            run_dir,
            "error",
            run_id=metadata["run_id"],
            gap_us=gap_us,
            target_packets=target_packets,
            error=str(exc),
        )
        raise


def archive_partial_attempt(run_dir: Path, *, stamp: str | None = None) -> dict[str, str]:
    stamp = stamp or _stamp_now()
    destination = run_dir / "partial" / stamp
    destination.mkdir(parents=True, exist_ok=False)
    archived: dict[str, str] = {}
    for name in ("rx.log", "tx.log", "run_state.json", "result.json"):
        source = run_dir / name
        if source.exists():
            target = destination / name
            shutil.move(str(source), str(target))
            archived[name.replace(".", "_")] = str(target)
    # Friendly keys used by callers/tests.
    if "rx_log" not in archived and (destination / "rx.log").exists():
        archived["rx_log"] = str(destination / "rx.log")
    if "tx_log" not in archived and (destination / "tx.log").exists():
        archived["tx_log"] = str(destination / "tx.log")
    return archived


def _execute(entry: dict, *, extra_args: list[str] | None = None) -> subprocess.CompletedProcess:
    env = os.environ.copy()
    env.update(entry.get("env", {}))
    command = list(entry["build_command"]) + list(extra_args or [])
    return subprocess.run(command, env=env, check=False)


def prebuild_matrix(matrix: dict, session_root: Path) -> dict:
    entries = [matrix["safe"], matrix["rx"], *matrix["tx"]]
    records: list[dict] = []
    for entry in entries:
        completed = _execute(entry)
        record = {
            "environment": entry["environment"],
            "gap_us": entry.get("gap_us"),
            "returncode": completed.returncode,
            "build_dir": entry["build_dir"],
        }
        # PLATFORMIO_BUILD_DIR contains the usual per-environment subfolder.
        firmware_bin = Path(entry["build_dir"]) / entry["environment"] / "firmware.bin"
        if firmware_bin.exists():
            record["firmware_bin"] = str(firmware_bin)
            record["firmware_bin_sha256"] = _sha256(firmware_bin)
        records.append(record)
        if completed.returncode != 0:
            break
    document = {"created_utc": _utc_now(), "builds": records}
    (session_root / "build_results.json").write_text(json.dumps(document, indent=2), encoding="utf-8")
    return document


def prepare_session(
    session_root: Path,
    project_dir: Path,
    *,
    rx_port: str,
    tx_port: str,
    target_packets: int,
    firmware_sha: str = FROZEN_FIRMWARE_SHA,
    prebuild: bool = False,
) -> dict:
    session_root.mkdir(parents=True, exist_ok=True)
    identity = collect_build_identity(project_dir)
    session = build_session_metadata(
        firmware_sha=firmware_sha,
        target_packets=target_packets,
        rx_port=rx_port,
        tx_port=tx_port,
        identity=identity,
    )
    session_id = f"pr1-{session['created_utc'].replace(':', '').replace('-', '')}"
    session["session_id"] = session_id
    (session_root / "session.json").write_text(json.dumps(session, indent=2), encoding="utf-8")

    runs = create_plan(session_root, firmware_sha, target_packets)
    for meta in runs:
        meta["session_id"] = session_id
        meta["build_identity"] = identity
        meta["ports"] = {"rx": rx_port, "tx": tx_port}
        run_dir = session_root / "runs" / str(meta["run_id"])
        (run_dir / "metadata.json").write_text(json.dumps(meta, indent=2), encoding="utf-8")
    (session_root / "manifest.json").write_text(
        json.dumps({"schema_version": 1, "runs": runs}, indent=2), encoding="utf-8"
    )

    matrix = prepare_build_matrix(session_root, project_dir)
    output = {"session": session, "build_matrix": matrix}
    if prebuild:
        output["build_results"] = prebuild_matrix(matrix, session_root)
    return output


def _serial_module():
    try:
        import serial  # type: ignore
    except ImportError as exc:
        raise RuntimeError(
            "pyserial is required for hardware capture; install PlatformIO (bundles pyserial) or `python -m pip install pyserial`"
        ) from exc
    return serial


def _open_serial(port: str):
    serial = _serial_module()
    ser = serial.Serial()
    ser.port = port
    ser.baudrate = SERIAL_BAUD
    ser.timeout = 0.20
    ser.write_timeout = 1.0
    # Open with both control lines low so opening never pulses the
    # USB-Serial/JTAG reset or strap logic.
    ser.dtr = False
    ser.rts = False
    ser.open()
    return ser


def _verify_safe_boot(port: str, timeout_s: float = 6.0) -> None:
    ser = _open_serial(port)
    try:
        # Boot output printed before the host opened the port is lost, so
        # reset with the port open and require a fresh boot block.
        _drain_stale_input(ser)
        _pulse_reset(ser)
        deadline = time.monotonic() + timeout_s
        saw_boot = False
        while time.monotonic() < deadline:
            raw = ser.readline()
            if not raw:
                continue
            line = _decode_line(raw).strip()
            if line == "PR1_RUNTIME_BOOT":
                saw_boot = True
            if line == "PR1_RUNTIME_SAFE_IDLE" and saw_boot:
                return
        raise TimeoutError(f"safe boot not verified on {port}")
    finally:
        ser.close()


def _drain_stale_input(serial_obj, *, settle_s: float = 0.30) -> None:
    """Drop output the device buffered while the port was closed.

    USB-Serial/JTAG delivers it a moment after open, so wait before flushing;
    otherwise an old boot block could be mistaken for the fresh one.
    """
    time.sleep(settle_s)
    serial_obj.reset_input_buffer()


def _pulse_reset(serial_obj, *, sleep_fn: Callable[[float], None] = time.sleep) -> None:
    """Best-effort USB/UART reset pulse; fresh boot is still mandatory afterward."""
    # Windows usbser.sys only sends SET_CONTROL_LINE_STATE when DTR is written,
    # so an RTS-only change never reaches the ESP32-S3 USB-Serial/JTAG (same
    # workaround as esptool). DTR must stay low at release or the ROM enters
    # download mode instead of booting the app.
    try:
        serial_obj.dtr = False
        serial_obj.rts = True
        serial_obj.dtr = False
        sleep_fn(0.10)
        serial_obj.rts = False
        serial_obj.dtr = False
        sleep_fn(0.35)
    except Exception as exc:
        raise RuntimeError(f"automatic RX reset pulse failed: {exc}") from exc


def run_hardware_session(
    session_root: Path,
    project_dir: Path,
    *,
    rx_port: str,
    tx_port: str,
    safe_first: bool,
    manual_rx_reset: bool,
) -> dict:
    """Execute the physical sweep, resuming completed run directories."""
    if not (session_root / "manifest.json").exists():
        prepare_session(
            session_root,
            project_dir,
            rx_port=rx_port,
            tx_port=tx_port,
            target_packets=1000,
        )
    matrix = prepare_build_matrix(session_root, project_dir)

    if safe_first:
        for port in (rx_port, tx_port):
            completed = _execute(matrix["safe"], extra_args=["--target", "upload", "--upload-port", port])
            if completed.returncode != 0:
                raise RuntimeError(f"safe upload failed on {port}")
            _verify_safe_boot(port)

    rx_upload = _execute(
        matrix["rx"], extra_args=["--target", "upload", "--upload-port", rx_port]
    )
    if rx_upload.returncode != 0:
        raise RuntimeError("fixed RX upload failed")

    rx_ser = _open_serial(rx_port)
    completed_this_process = 0
    try:
        manifest = json.loads((session_root / "manifest.json").read_text(encoding="utf-8"))["runs"]
        by_gap = {int(entry["gap_us"]): entry for entry in matrix["tx"]}
        for meta in manifest:
            run_dir = session_root / "runs" / str(meta["run_id"])
            result_path = run_dir / "result.json"
            if result_path.exists():
                result = json.loads(result_path.read_text(encoding="utf-8"))
                if result.get("derived", {}).get("target_reached"):
                    continue

            # Preserve evidence from any previous failed/partial attempt before retry.
            if (run_dir / "rx.log").exists() or (run_dir / "tx.log").exists():
                archive_partial_attempt(run_dir)

            gap_us = int(meta["gap_us"])
            tx_entry = by_gap[gap_us]
            tx_upload = _execute(
                tx_entry, extra_args=["--target", "upload", "--upload-port", tx_port]
            )
            if tx_upload.returncode != 0:
                raise RuntimeError(f"TX upload failed for gap {gap_us} us")

            # The TX boot block printed right after upload is lost before the
            # port opens; reset the same image with the port open to capture it.
            tx_ser = _open_serial(tx_port)
            try:
                _drain_stale_input(tx_ser)
                _pulse_reset(tx_ser)

                # Reset RX only after the new-gap TX image is running. Resetting
                # before the TX upload lets the previous gap's TX seed the fresh RX
                # sequence reference; the new TX then restarts at seq 0, lands
                # BeforeOrigin, and is counted in crc_good but never in missing.
                # Stale input is dropped so the boot markers read next are fresh.
                _drain_stale_input(rx_ser)
                if manual_rx_reset:
                    print(f"Reset RX board on {rx_port}, then press Enter to continue.", flush=True)
                    input()
                else:
                    _pulse_reset(rx_ser)

                capture_run_from_serial(meta, run_dir, rx_ser, tx_ser)
            finally:
                tx_ser.close()
            completed_this_process += 1
    finally:
        rx_ser.close()

    report = analyze_directory(session_root)
    return {
        "status": experiment_status(session_root),
        "analysis": report["analysis"],
        "report_dir": str(session_root / "report"),
    }


def _print_command(command: list[str], env: dict | None = None) -> str:
    prefix = " ".join(f"{key}={value}" for key, value in (env or {}).items())
    body = subprocess.list2cmdline(command)
    return f"{prefix} {body}".strip()


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest="command", required=True)

    prepare = sub.add_parser("prepare", help="create metadata, generated configs, and optional build caches")
    prepare.add_argument("session_root", type=Path)
    prepare.add_argument("project_dir", type=Path)
    prepare.add_argument("--rx-port", required=True)
    prepare.add_argument("--tx-port", required=True)
    prepare.add_argument("--target-packets", type=int, default=1000)
    prepare.add_argument("--firmware-sha", default=FROZEN_FIRMWARE_SHA)
    prepare.add_argument("--prebuild", action="store_true")

    dry = sub.add_parser("dry-run", help="print the complete physical sequence without hardware actions")
    dry.add_argument("session_root", type=Path)
    dry.add_argument("project_dir", type=Path)
    dry.add_argument("--rx-port", required=True)
    dry.add_argument("--tx-port", required=True)
    dry.add_argument("--safe-first", action="store_true")

    run = sub.add_parser("run", help="run/resume the physical sweep and final analysis")
    run.add_argument("session_root", type=Path)
    run.add_argument("project_dir", type=Path)
    run.add_argument("--rx-port", required=True)
    run.add_argument("--tx-port", required=True)
    run.add_argument("--safe-first", action="store_true")
    run.add_argument("--manual-rx-reset", action="store_true")

    args = parser.parse_args(argv)
    try:
        if args.command == "prepare":
            document = prepare_session(
                args.session_root,
                args.project_dir,
                rx_port=args.rx_port,
                tx_port=args.tx_port,
                target_packets=args.target_packets,
                firmware_sha=args.firmware_sha,
                prebuild=args.prebuild,
            )
            compact = {
                "session": document["session"],
                "prebuild_requested": args.prebuild,
                "build_results": document.get("build_results"),
            }
            print(json.dumps(compact, indent=2))
        elif args.command == "dry-run":
            plan = build_dry_run_plan(
                project_dir=args.project_dir,
                session_root=args.session_root,
                rx_port=args.rx_port,
                tx_port=args.tx_port,
                include_safe=args.safe_first,
            )
            print(json.dumps(plan, indent=2))
        else:
            result = run_hardware_session(
                args.session_root,
                args.project_dir,
                rx_port=args.rx_port,
                tx_port=args.tx_port,
                safe_first=args.safe_first,
                manual_rx_reset=args.manual_rx_reset,
            )
            print(json.dumps(result, indent=2))
    except KeyboardInterrupt:
        print("pr1_experiment_controller: interrupted; partial run evidence was preserved", file=sys.stderr)
        return 130
    except (OSError, RuntimeError, TimeoutError, ValueError, KeyError, json.JSONDecodeError) as exc:
        print(f"pr1_experiment_controller: {exc}", file=sys.stderr)
        return 2
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
