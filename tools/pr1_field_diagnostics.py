"""Read-only PR1 field-run audit. No USB, RF, board flashing, or third-party packages.

Usage: python tools/pr1_field_diagnostics.py runs/.../5_m_LOS_repeat runs/.../5_m_body --format md
"""
import argparse
import hashlib
import json
import re
import sys
from pathlib import Path

P99_FIELDS = ("irq_to_spi_us", "spi_duration_us", "rx_processing_us",
              "spi_end_to_rearm_start_us", "rx_rearm_us", "irq_to_rx_ready_us")
FIELD = re.compile(r"^PR1T\s+.*?\bfield=(\w+)\s+value=(-?\d+)\s*$")


def _nonnegative(data, key):
    value = data.get(key)
    if type(value) is not int or value < 0:
        raise ValueError(f"{key} must be a nonnegative integer")
    return value


def _optional_float(data, key):
    value = data.get(key)
    if value is None:
        return None
    if isinstance(value, bool) or not isinstance(value, (int, float)):
        raise ValueError(f"{key} must be numeric or absent")
    return float(value)


def _read_telemetry(path):
    fields = {}
    if path.exists():
        for line in path.read_text(encoding="utf-8").splitlines():
            match = FIELD.match(line.strip())
            if match:
                fields[match.group(1)] = int(match.group(2))
    return fields


def analyze_run(run_directory):
    """Normalize historical flat or nested summary; do not invent missing measurements."""
    directory = Path(run_directory)
    summary_path = directory / "summary.json"
    source = json.loads(summary_path.read_text(encoding="utf-8"))
    rf = source.get("rf") if isinstance(source.get("rf"), dict) else source
    audio = source.get("audio") if isinstance(source.get("audio"), dict) else {}
    good = _nonnegative(rf, "crc_good")
    bad = _nonnegative(rf, "crc_bad")
    missing = _nonnegative(rf, "missing")
    if good + missing == 0:
        raise ValueError("no known successfully decoded or missing packets")
    loss_pct = 100.0 * missing / (good + missing)
    crc_pct = 100.0 * bad / (good + bad) if good + bad else None
    recorded_loss = _optional_float(rf, "loss_pct")
    warnings = []
    if recorded_loss is not None and abs(recorded_loss - loss_pct) > 0.02:
        warnings.append("recorded loss_pct differs by >0.02 percentage points; recomputed from counters")
    log_path = directory / "rx_pull.log"
    fields = _read_telemetry(log_path)
    concealed_pct = _optional_float(source, "concealed_pct")
    if concealed_pct is None:
        concealed_pct = _optional_float(audio, "concealed_pct")
    last_rssi = _optional_float(rf, "rssi_dbm")
    if last_rssi is None and "rssi_dbm" in fields:
        last_rssi = float(fields["rssi_dbm"])
    hashes = {"summary.json": hashlib.sha256(summary_path.read_bytes()).hexdigest()}
    if log_path.exists():
        hashes["rx_pull.log"] = hashlib.sha256(log_path.read_bytes()).hexdigest()
    return {
        "run": str(directory), "label": str(source.get("label") or directory.name),
        "window_s": _optional_float(source, "window_s"),
        "crc_good": good, "crc_bad": bad, "missing": missing,
        "rf_loss_pct": loss_pct, "crc_bad_among_decoded_pct": crc_pct,
        "last_good_packet_rssi_dbm": last_rssi, "concealed_pct": concealed_pct,
        "recent_64_p99_us": {key: fields.get(key) for key in P99_FIELDS},
        "trace_ring_overwrites": fields.get("trace_overwrites"),
        "firmware_reset_count": source.get("resets"),
        "source_sha256": hashes, "warnings": warnings,
        "limitations": (
            "RSSI is the last successful packet only, not median/p10/p90. "
            "Timing percentiles summarize the recent 64 observations only, not whole run. "
            "CRC-bad and missing counts can overlap; never add their percentages. "
            "Trace overwrites represent RAM history eviction, not RF losses. "
            "No TX-side ground truth, channel occupancy, or controlled geometry supplied."
        ),
    }


def compare_runs(baseline, candidate):
    """Descriptive differences. Never interpret uncontrolled runs as causal tests."""
    def delta(key):
        a, b = baseline.get(key), candidate.get(key)
        return b - a if a is not None and b is not None else None
    return {
        "baseline": baseline["label"], "candidate": candidate["label"],
        "rf_loss_change_pp": delta("rf_loss_pct"),
        "crc_bad_change_pp": delta("crc_bad_among_decoded_pct"),
        "last_packet_rssi_change_db": delta("last_good_packet_rssi_dbm"),
        "concealed_change_pp": delta("concealed_pct"),
        "causal_inference": "not established by these logs",
    }


def _fmt(value, decimals=2):
    return "n/a" if value is None else f"{value:.{decimals}f}"


def _markdown(rows):
    lines = ["# PR1 field RF diagnostics (read-only)", "",
             "| Run | RF missing (%) | CRC bad / decoded (%) | RSSI last good (dBm) | Concealed (%) |",
             "|---|---:|---:|---:|---:|"]
    for r in rows:
        name = r["label"].replace("|", "\\|")
        lines.append(f'| {name} | {_fmt(r["rf_loss_pct"],3)} | '
                     f'{_fmt(r["crc_bad_among_decoded_pct"],3)} | '
                     f'{_fmt(r["last_good_packet_rssi_dbm"],0)} | '
                     f'{_fmt(r["concealed_pct"],3)} |')
    lines += ["", "**Measurement constraints:** " + rows[0]["limitations"]] if rows else []
    for r in rows:
        for warning in r["warnings"]:
            lines.append(f'- {r["label"]}: {warning}')
    return "\n".join(lines) + "\n"


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("runs", type=Path, nargs="+", help="run directories with summary.json")
    parser.add_argument("--format", choices=("json", "md"), default="md")
    args = parser.parse_args(argv)
    try:
        rows = [analyze_run(p) for p in args.runs]
    except (OSError, ValueError, json.JSONDecodeError) as exc:
        parser.exit(2, f"pr1_field_diagnostics: {exc}\n")
    if args.format == "json":
        print(json.dumps({"schema_version": 1, "runs": rows}, ensure_ascii=False, indent=2))
    else:
        print(_markdown(rows), end="")
    return 0


if __name__ == "__main__":
    sys.exit(main())
