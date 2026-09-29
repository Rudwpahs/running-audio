"""Diagnostic 10k runs (not tool-marked revalidation): near-transition gap + 5000 us control.

Host orchestration only. Reuses the baseline session's cached TX builds and the
controller's capture/validation/parse path unchanged; RX keeps the fixed image
already flashed by the baseline run and is only reset (fresh boot verified).
"""
import json
import sys
from pathlib import Path

REPO = Path(r"C:\Users\USER\Projects\running-audio")
sys.path.insert(0, str(REPO / "tools"))

import pr1_experiment_controller as ctl  # noqa: E402
from pr1_board_sweep import build_run_metadata, write_sweep_outputs  # noqa: E402

RX_PORT, TX_PORT = "COM3", "COM4"
TARGET = int(__import__("os").environ.get("PR1_TARGET", "10000"))
root = Path(__file__).resolve().parent
base = REPO / "runs" / "pr1-board-test"

gaps = [int(g) for g in sys.argv[1:]]  # diagnostic gaps given explicitly

matrix = json.loads((base / "build_matrix.json").read_text(encoding="utf-8"))
by_gap = {int(e["gap_us"]): e for e in matrix["tx"]}
identity = ctl.collect_build_identity(REPO / "firmware" / "t3s3_sx1280_runtime", source_root=REPO)

session = {
    "created_utc": ctl._utc_now(),
    "purpose": "gate5_distance", "distance_m": 1.0, "distance_source": "operator-reported", "previous_runs_distance_m": 0.5,
    "source_session": str(base),
    "revalidate_gaps_us": gaps,
    "target_packets": TARGET,
    "ports": {"rx": RX_PORT, "tx": TX_PORT},
    "build_identity": identity,
    "driver_file_sha256": ctl._sha256(Path(__file__)),
}
(root / "session.json").write_text(json.dumps(session, indent=2), encoding="utf-8")

results = []
rx_ser = ctl._open_serial(RX_PORT)
try:
    for index, gap in enumerate(gaps, start=1):
        run_id = f"d{index:02d}-gap-{gap}us-{TARGET // 1000}k"
        run_dir = root / "runs" / run_id
        run_dir.mkdir(parents=True, exist_ok=True)
        result_path = run_dir / "result.json"
        if result_path.exists():
            prior = json.loads(result_path.read_text(encoding="utf-8"))
            if prior.get("derived", {}).get("target_reached"):
                results.append(prior)
                continue
        if (run_dir / "rx.log").exists() or (run_dir / "tx.log").exists():
            ctl.archive_partial_attempt(run_dir)

        meta = build_run_metadata(gap, target_packets=TARGET, phase="revalidate",
                                  firmware_sha=ctl.FROZEN_FIRMWARE_SHA, run_id=run_id)
        meta.update({"build_identity": identity, "ports": session["ports"]})
        (run_dir / "metadata.json").write_text(json.dumps(meta, indent=2), encoding="utf-8")

        up = ctl._execute(by_gap[gap], extra_args=["--target", "upload", "--upload-port", TX_PORT])
        if up.returncode != 0:
            raise RuntimeError(f"TX upload failed for gap {gap} us")
        tx_ser = ctl._open_serial(TX_PORT)
        try:
            ctl._drain_stale_input(tx_ser)
            ctl._pulse_reset(tx_ser)
            ctl._drain_stale_input(rx_ser)
            ctl._pulse_reset(rx_ser)
            results.append(ctl.capture_run_from_serial(meta, run_dir, rx_ser, tx_ser, max_polls=600))
        finally:
            tx_ser.close()
        print(f"RUN {gap} complete {results[-1]['packet_count']}", flush=True)
finally:
    rx_ser.close()

write_sweep_outputs(results, root / "report")
print("REVALIDATE_DONE", flush=True)
