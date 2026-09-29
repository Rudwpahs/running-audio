"""Gate 5 distance runs with an untethered TX (powered from a USB power bank).

Host orchestration only, frozen firmware unchanged.

  pre  <dist_m> <gap_us>          TX on COM4: reset, capture + validate fresh boot
                                  (role tx, frozen profile, tx_gap_us) -> tx_pre_move.log
  run  <dist_m> <gap_us> <target> TX already moved to the power bank; RX on COM3 is
                                  reset (fresh boot validated) and captured to target.

TX pull telemetry (scheduler_misses) is unavailable while the TX is untethered;
it is recorded as null with a reason, never filled in.
"""
import json
import sys
import time
from pathlib import Path

REPO = Path(r"C:\Users\USER\Projects\running-audio")
sys.path.insert(0, str(REPO / "tools"))

import pr1_experiment_controller as ctl  # noqa: E402
from pr1_board_sweep import (  # noqa: E402
    RX_FIELD_MAP, _parse_kv_and_telemetry, _validate_live_profile,
    build_run_metadata, classify_bottleneck,
)

RX_PORT, TX_PORT = "COM3", "COM4"
HERE = Path(__file__).resolve().parent


def run_dir_for(dist: str, gap: int) -> Path:
    d = HERE / f"{dist}m" / f"gap-{gap}us"
    d.mkdir(parents=True, exist_ok=True)
    return d


def stage_pre(dist: str, gap: int) -> None:
    run_dir = run_dir_for(dist, gap)
    ser = ctl._open_serial(TX_PORT)
    try:
        with (run_dir / "tx_pre_move.log").open("w", encoding="utf-8", buffering=1) as fh:
            ctl._drain_stale_input(ser)
            ctl._pulse_reset(ser)
            boot = ctl._read_until_marker(ser, fh, "PR1_RUNTIME_LIVE_READY", 10.0)
    finally:
        ser.close()
    if "PR1_RUNTIME_BOOT" not in (line.strip() for line in boot):
        raise TimeoutError("tx LIVE_READY without fresh PR1_RUNTIME_BOOT")
    meta, _ = _parse_kv_and_telemetry(boot)
    _validate_live_profile(meta, role="tx", expected_gap_us=gap, source="tx")
    print(f"TX_PRE_OK dist={dist}m gap={gap}us", flush=True)


def stage_run(dist: str, gap: int, target: int) -> None:
    run_dir = run_dir_for(dist, gap)
    if not (run_dir / "tx_pre_move.log").exists():
        raise RuntimeError("run 'pre' first: no validated TX boot for this point")
    for name in ("rx.log", "run_state.json", "result.json"):
        if (run_dir / name).exists():
            ctl.archive_partial_attempt(run_dir)
            break
    meta = build_run_metadata(gap, target_packets=target, phase="revalidate",
                              firmware_sha=ctl.FROZEN_FIRMWARE_SHA,
                              run_id=f"dist-{dist}m-gap-{gap}us-{target // 1000}k")
    meta.update({
        "gate": "gate5_distance", "distance_m": float(dist), "distance_source": "operator-reported",
        "tx_power": "usb_power_bank_untethered", "ports": {"rx": RX_PORT, "tx": None},
        "build_identity": ctl.collect_build_identity(REPO / "firmware" / "t3s3_sx1280_runtime", source_root=REPO),
    })
    (run_dir / "metadata.json").write_text(json.dumps(meta, indent=2), encoding="utf-8")
    ctl._write_state(run_dir, "running", run_id=meta["run_id"], started_utc=ctl._utc_now())

    rx = ctl._open_serial(RX_PORT)
    try:
        with (run_dir / "rx.log").open("w", encoding="utf-8", buffering=1) as fh:
            ctl._drain_stale_input(rx)
            ctl._pulse_reset(rx)
            boot = ctl._read_until_marker(rx, fh, "PR1_RUNTIME_LIVE_READY", 10.0)
            if "PR1_RUNTIME_BOOT" not in (line.strip() for line in boot):
                raise TimeoutError("rx LIVE_READY without fresh PR1_RUNTIME_BOOT")
            rx_meta, _ = _parse_kv_and_telemetry(boot)
            _validate_live_profile(rx_meta, role="rx", expected_gap_us=None, source="rx")

            time.sleep(ctl.estimate_initial_wait_s(gap, target))
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
            time.sleep(max(0.25, ctl.estimate_initial_wait_s(gap, 128)))
            ctl._snapshot_with_recovery(rx, fh, 1.0)
    except Exception as exc:
        ctl._write_state(run_dir, "error", run_id=meta["run_id"], error=str(exc))
        raise
    finally:
        rx.close()

    _, tel = _parse_kv_and_telemetry((run_dir / "rx.log").read_text(encoding="utf-8", errors="replace").splitlines())
    metrics = {out: tel.get(raw) for raw, out in RX_FIELD_MAP.items()}
    metrics["scheduler_misses"] = None
    good, missing, bad = metrics["crc_good"], metrics["missing"], metrics["crc_bad"]
    count = good + missing
    loss = missing / count if count else 0.0
    result = {
        "run_id": meta["run_id"], "phase": "gate5_distance", "gap_us": gap,
        "distance_m": float(dist), "target_packets": target, "packet_count": count,
        "metrics": metrics,
        "derived": {"loss_rate": loss, "crc_bad_rate": (bad / count) if count else 0.0,
                    "target_reached": count >= target},
        "classification": classify_bottleneck(metrics, loss),
        "evidence": {
            "packet_count_source": "rx_sequence_span=crc_good+missing",
            "scheduler_misses": "unavailable: TX untethered on power bank; TX boot validated pre-move (tx_pre_move.log)",
            "evidence_boundary": "measured_diagnostics_not_causal_proof",
        },
    }
    (run_dir / "result.json").write_text(json.dumps(result, indent=2), encoding="utf-8")
    ctl._write_state(run_dir, "complete", run_id=meta["run_id"], packet_count=count, finished_utc=ctl._utc_now())
    print(f"RUN dist={dist}m gap={gap}us packets={count} missing={missing} loss={100*loss:.3f}% "
          f"crc_bad={bad} rssi={metrics['rssi_dbm']}", flush=True)


if __name__ == "__main__":
    stage, dist, gap = sys.argv[1], sys.argv[2], int(sys.argv[3])
    if stage == "pre":
        stage_pre(dist, gap)
    else:
        stage_run(dist, gap, int(sys.argv[4]))
