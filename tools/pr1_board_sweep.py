#!/usr/bin/env python3
"""Host-side planning, parsing, analysis, and report generation for the PR1 SX1280 sweep."""

from __future__ import annotations

import argparse
import csv
import json
import math
import shlex
import subprocess
from datetime import datetime, timezone
from pathlib import Path
from typing import Iterable, Sequence

SWEEP_GAPS_US = [5000, 1000, 500, 300, 250, 225, 200, 175, 150, 125, 0]
SCHEMA_VERSION = 1

FROZEN_BASELINE = {
    "frequency_mhz": 2404.0,
    "bitrate_kbps": 1300,
    "coding_rate": 3,
    "output_dbm": 0,
    "packet_bytes": 116,
    "sx1280_spi_hz": 2_000_000,
    "adaptive_layers": False,
}

RX_FIELD_MAP = {
    "rssi_dbm": "rssi_dbm",
    "crc_good": "crc_good",
    "crc_bad": "crc_bad",
    "missing": "missing",
    "queue_depth": "queue_depth",
    "max_queue_depth": "max_queue_depth",
    "irq_to_spi_us": "irq_to_spi_us_p99",
    "spi_duration_us": "spi_duration_us_p99",
    "rx_processing_us": "rx_processing_us_p99",
    "spi_end_to_rearm_start_us": "spi_end_to_rearm_start_us_p99",
    "rx_rearm_us": "rx_rearm_us_p99",
    "irq_to_rx_ready_us": "irq_to_rx_ready_us_p99",
    "trace_overwrites": "trace_overwrites",
}

RESULT_CSV_FIELDS = [
    "run_id", "phase", "gap_us", "target_packets", "packet_count",
    "missing", "loss_rate", "crc_good", "crc_bad", "crc_bad_rate",
    "rssi_dbm", "queue_depth", "max_queue_depth", "scheduler_misses",
    "irq_to_spi_us_p99", "spi_duration_us_p99", "rx_processing_us_p99",
    "spi_end_to_rearm_start_us_p99", "rx_rearm_us_p99",
    "irq_to_rx_ready_us_p99", "trace_overwrites",
    "bottleneck", "confidence", "revalidate_10000",
]


def _utc_now() -> str:
    return datetime.now(timezone.utc).isoformat().replace("+00:00", "Z")


def build_run_metadata(
    gap_us: int,
    *,
    target_packets: int,
    phase: str,
    firmware_sha: str,
    run_id: str | None = None,
) -> dict:
    if gap_us not in SWEEP_GAPS_US:
        raise ValueError(f"unsupported sweep gap_us: {gap_us}")
    if target_packets <= 0:
        raise ValueError("target_packets must be positive")
    if phase not in {"baseline", "revalidate"}:
        raise ValueError("phase must be baseline or revalidate")
    return {
        "schema_version": SCHEMA_VERSION,
        "run_id": run_id or f"gap-{gap_us:04d}us-{phase}",
        "created_utc": _utc_now(),
        "gap_us": gap_us,
        "target_packets": target_packets,
        "phase": phase,
        "firmware_sha": firmware_sha,
        "frozen_baseline": dict(FROZEN_BASELINE),
        "files": {"rx_log": "rx.log", "tx_log": "tx.log"},
        "notes": "",
    }


def _parse_kv_and_telemetry(lines: Iterable[str]) -> tuple[dict[str, str], dict[str, int]]:
    metadata: dict[str, str] = {}
    telemetry: dict[str, int] = {}
    for raw in lines:
        line = raw.strip()
        if not line:
            continue
        if line.startswith("PR1T "):
            values: dict[str, str] = {}
            for token in line.split()[1:]:
                if "=" in token:
                    key, value = token.split("=", 1)
                    values[key] = value
            field = values.get("field")
            value = values.get("value")
            if field and value is not None:
                try:
                    telemetry[field] = int(value, 10)
                except ValueError as exc:
                    raise ValueError(f"invalid telemetry integer for {field}: {value}") from exc
            continue
        if line.startswith("PR1") or "=" not in line:
            continue
        key, value = line.split("=", 1)
        metadata[key] = value
    return metadata, telemetry


def _expect_text(meta: dict[str, str], key: str, expected: str, source: str) -> None:
    actual = meta.get(key)
    if actual != expected:
        raise ValueError(f"{source} {key}: expected {expected}, got {actual}")


def _expect_int(meta: dict[str, str], key: str, expected: int, source: str) -> None:
    actual = meta.get(key)
    try:
        parsed = int(actual) if actual is not None else None
    except ValueError:
        parsed = None
    if parsed != expected:
        raise ValueError(f"{source} {key}: expected {expected}, got {actual}")


def _expect_float(meta: dict[str, str], key: str, expected: float, source: str) -> None:
    actual = meta.get(key)
    try:
        parsed = float(actual) if actual is not None else None
    except ValueError:
        parsed = None
    if parsed is None or not math.isclose(parsed, expected, rel_tol=0.0, abs_tol=0.001):
        raise ValueError(f"{source} {key}: expected {expected}, got {actual}")


def _validate_live_profile(meta: dict[str, str], *, role: str, expected_gap_us: int | None, source: str) -> None:
    _expect_text(meta, "runtime_role", role, source)
    _expect_int(meta, "rf_enabled", 1, source)
    _expect_int(meta, "sx1280_spi_hz", FROZEN_BASELINE["sx1280_spi_hz"], source)
    _expect_float(meta, "frequency_mhz", FROZEN_BASELINE["frequency_mhz"], source)
    _expect_int(meta, "bitrate_kbps", FROZEN_BASELINE["bitrate_kbps"], source)
    _expect_int(meta, "coding_rate", FROZEN_BASELINE["coding_rate"], source)
    _expect_int(meta, "output_dbm", FROZEN_BASELINE["output_dbm"], source)
    _expect_int(meta, "packet_bytes", FROZEN_BASELINE["packet_bytes"], source)
    _expect_text(meta, "adaptive_layers", "off", source)
    if expected_gap_us is not None:
        _expect_int(meta, "tx_gap_us", expected_gap_us, source)


def _classify(metrics: dict[str, int | None], loss_rate: float) -> dict[str, str | float | None]:
    total = metrics.get("irq_to_rx_ready_us_p99")
    components = {
        "scheduler_wakeup": metrics.get("irq_to_spi_us_p99"),
        "spi_transaction": metrics.get("spi_duration_us_p99"),
        "post_spi_processing": metrics.get("spi_end_to_rearm_start_us_p99"),
        "rx_rearm": metrics.get("rx_rearm_us_p99"),
    }
    present = {k: v for k, v in components.items() if isinstance(v, int)}
    if loss_rate <= 0:
        return {"label": "no_loss_observed", "confidence": "low", "dominant_share": None}
    if not isinstance(total, int) or total <= 0 or len(present) < 4:
        return {"label": "insufficient_timing_evidence", "confidence": "low", "dominant_share": None}
    label, value = max(present.items(), key=lambda item: item[1])
    share = value / total
    if share >= 0.45:
        return {"label": label, "confidence": "low", "dominant_share": round(share, 4)}
    return {"label": "receiver_turnaround_mixed", "confidence": "low", "dominant_share": round(share, 4)}


def parse_run_logs(metadata: dict, rx_lines: Iterable[str], tx_lines: Iterable[str] | None = None) -> dict:
    rx_meta, rx_tel = _parse_kv_and_telemetry(rx_lines)
    tx_meta: dict[str, str] = {}
    tx_tel: dict[str, int] = {}
    if tx_lines is not None:
        tx_meta, tx_tel = _parse_kv_and_telemetry(tx_lines)

    gap_us = int(metadata["gap_us"])
    _validate_live_profile(rx_meta, role="rx", expected_gap_us=None, source="rx")
    if tx_lines is not None:
        _validate_live_profile(tx_meta, role="tx", expected_gap_us=gap_us, source="tx")

    metrics: dict[str, int | None] = {}
    for raw_name, result_name in RX_FIELD_MAP.items():
        metrics[result_name] = rx_tel.get(raw_name)
    metrics["scheduler_misses"] = tx_tel.get("scheduler_misses", rx_tel.get("scheduler_misses"))

    crc_good = metrics.get("crc_good")
    missing = metrics.get("missing")
    if not isinstance(crc_good, int) or not isinstance(missing, int):
        raise ValueError("rx log must contain crc_good and missing telemetry")

    # missing is derived from successful sequence gaps. A CRC-failed packet can later
    # appear in that gap, so crc_bad must not be added again to the packet span.
    packet_count = crc_good + missing
    loss_rate = (missing / packet_count) if packet_count else 0.0
    crc_bad = metrics.get("crc_bad")
    crc_bad_rate = (crc_bad / packet_count) if isinstance(crc_bad, int) and packet_count else 0.0
    classification = _classify(metrics, loss_rate)

    return {
        "schema_version": SCHEMA_VERSION,
        "run_id": metadata["run_id"],
        "phase": metadata["phase"],
        "gap_us": gap_us,
        "target_packets": int(metadata["target_packets"]),
        "firmware_sha": metadata.get("firmware_sha", ""),
        "packet_count": packet_count,
        "metrics": metrics,
        "derived": {
            "loss_rate": loss_rate,
            "crc_bad_rate": crc_bad_rate,
            "target_reached": packet_count >= int(metadata["target_packets"]),
        },
        "classification": classification,
        "evidence": {
            "packet_count_source": "rx_sequence_span=crc_good+missing",
            "crc_bad_overlap_warning": "crc_bad may overlap sequence-derived missing and is not additive",
            "scheduler_misses_source": "tx" if "scheduler_misses" in tx_tel else "rx_or_unobserved",
        },
        "revalidate_10000": False,
    }


def render_tx_sweep_config(base_text: str, gap_us: int) -> str:
    if gap_us not in SWEEP_GAPS_US:
        raise ValueError(f"unsupported sweep gap_us: {gap_us}")
    needle = "-D PR1_TX_GAP_US=5000"
    if base_text.count(needle) != 1:
        raise ValueError("platformio.ini must contain exactly one frozen gap macro: -D PR1_TX_GAP_US=5000")
    return base_text.replace(needle, f"-D PR1_TX_GAP_US={gap_us}", 1)


def build_flash_command(
    *, project_dir: Path, config_path: Path, environment: str, port: str
) -> list[str]:
    return [
        "pio", "run",
        "--project-dir", str(project_dir),
        "--project-conf", str(config_path),
        "--environment", environment,
        "--target", "upload",
        "--upload-port", port,
    ]


def _transition_threshold(a: dict, b: dict) -> tuple[float, float]:
    n1 = max(1, int(a["packet_count"]))
    n2 = max(1, int(b["packet_count"]))
    p1 = float(a["derived"]["loss_rate"])
    p2 = float(b["derived"]["loss_rate"])
    pooled = ((p1 * n1) + (p2 * n2)) / (n1 + n2)
    se = math.sqrt(max(0.0, pooled * (1.0 - pooled) * ((1.0 / n1) + (1.0 / n2))))
    return abs(p2 - p1), max(0.005, 3.0 * se)


def analyze_sweep(results: Sequence[dict]) -> dict:
    by_gap = {int(row["gap_us"]): row for row in results}
    ordered = [by_gap[gap] for gap in SWEEP_GAPS_US if gap in by_gap]
    missing_gaps = [gap for gap in SWEEP_GAPS_US if gap not in by_gap]
    transitions: list[dict] = []
    marked: set[int] = set()
    for higher_gap, lower_gap in zip(SWEEP_GAPS_US, SWEEP_GAPS_US[1:]):
        if higher_gap not in by_gap or lower_gap not in by_gap:
            continue
        higher, lower = by_gap[higher_gap], by_gap[lower_gap]
        delta, threshold = _transition_threshold(higher, lower)
        if delta >= threshold:
            transition = {
                "higher_gap_us": higher_gap,
                "lower_gap_us": lower_gap,
                "higher_loss_rate": float(higher["derived"]["loss_rate"]),
                "lower_loss_rate": float(lower["derived"]["loss_rate"]),
                "absolute_delta": delta,
                "material_threshold": threshold,
            }
            transitions.append(transition)
            marked.update((higher_gap, lower_gap))

    revalidate = [gap for gap in SWEEP_GAPS_US if gap in marked]
    bottleneck = {"label": "insufficient_transition_evidence", "confidence": "low", "evidence_gap_us": None}
    if transitions:
        strongest = max(transitions, key=lambda item: item["absolute_delta"])
        candidates = [
            row for row in ordered
            if int(row["gap_us"]) in {strongest["higher_gap_us"], strongest["lower_gap_us"]}
        ]
        if candidates:
            evidence_row = max(candidates, key=lambda row: float(row["derived"]["loss_rate"]))
            label = evidence_row.get("classification", {}).get("label", "insufficient_timing_evidence")
            confidence = "medium" if label not in {"no_loss_observed", "insufficient_timing_evidence"} else "low"
            bottleneck = {"label": label, "confidence": confidence, "evidence_gap_us": int(evidence_row["gap_us"])}
    return {
        "transition_rule": "abs(loss_delta) >= max(0.5 percentage points, 3*pooled-binomial-SE)",
        "missing_gaps_us": missing_gaps,
        "transitions": transitions,
        "revalidate_10000_gaps_us": revalidate,
        "bottleneck_summary": bottleneck,
    }


def _flatten(result: dict) -> dict:
    metrics = result["metrics"]
    return {
        "run_id": result["run_id"],
        "phase": result["phase"],
        "gap_us": result["gap_us"],
        "target_packets": result["target_packets"],
        "packet_count": result["packet_count"],
        "missing": metrics.get("missing"),
        "loss_rate": result["derived"]["loss_rate"],
        "crc_good": metrics.get("crc_good"),
        "crc_bad": metrics.get("crc_bad"),
        "crc_bad_rate": result["derived"]["crc_bad_rate"],
        "rssi_dbm": metrics.get("rssi_dbm"),
        "queue_depth": metrics.get("queue_depth"),
        "max_queue_depth": metrics.get("max_queue_depth"),
        "scheduler_misses": metrics.get("scheduler_misses"),
        "irq_to_spi_us_p99": metrics.get("irq_to_spi_us_p99"),
        "spi_duration_us_p99": metrics.get("spi_duration_us_p99"),
        "rx_processing_us_p99": metrics.get("rx_processing_us_p99"),
        "spi_end_to_rearm_start_us_p99": metrics.get("spi_end_to_rearm_start_us_p99"),
        "rx_rearm_us_p99": metrics.get("rx_rearm_us_p99"),
        "irq_to_rx_ready_us_p99": metrics.get("irq_to_rx_ready_us_p99"),
        "trace_overwrites": metrics.get("trace_overwrites"),
        "bottleneck": result["classification"]["label"],
        "confidence": result["classification"]["confidence"],
        "revalidate_10000": result.get("revalidate_10000", False),
    }


def _svg_chart(path: Path, rows: Sequence[dict], series: Sequence[tuple[str, str]], title: str, y_label: str) -> None:
    width, height = 960, 540
    left, right, top, bottom = 85, 30, 55, 75
    plot_w, plot_h = width - left - right, height - top - bottom
    xs = [float(r["gap_us"]) for r in rows]
    values = [float(r[key]) for r in rows for key, _ in series if r.get(key) is not None]
    if not xs:
        xs = [0.0, 1.0]
    if not values:
        values = [0.0, 1.0]
    x_min, x_max = min(xs), max(xs)
    y_min, y_max = min(0.0, min(values)), max(values)
    if math.isclose(x_min, x_max):
        x_max = x_min + 1.0
    if math.isclose(y_min, y_max):
        y_max = y_min + 1.0

    def sx(x: float) -> float:
        return left + (x - x_min) / (x_max - x_min) * plot_w

    def sy(y: float) -> float:
        return top + plot_h - (y - y_min) / (y_max - y_min) * plot_h

    parts = [
        f'<svg xmlns="http://www.w3.org/2000/svg" width="{width}" height="{height}" viewBox="0 0 {width} {height}">',
        '<rect width="100%" height="100%" fill="white"/>',
        f'<text x="{width/2}" y="30" text-anchor="middle" font-family="sans-serif" font-size="20">{title}</text>',
        f'<line x1="{left}" y1="{top+plot_h}" x2="{left+plot_w}" y2="{top+plot_h}" stroke="black"/>',
        f'<line x1="{left}" y1="{top}" x2="{left}" y2="{top+plot_h}" stroke="black"/>',
        f'<text x="{width/2}" y="{height-20}" text-anchor="middle" font-family="sans-serif" font-size="14">TX post-send gap (us)</text>',
        f'<text x="20" y="{height/2}" transform="rotate(-90 20 {height/2})" text-anchor="middle" font-family="sans-serif" font-size="14">{y_label}</text>',
    ]
    dash_patterns = ["", "6,4", "2,4", "10,4", "8,3,2,3", "4,2"]
    for idx, (key, label) in enumerate(series):
        points = [(sx(float(r["gap_us"])), sy(float(r[key]))) for r in rows if r.get(key) is not None]
        if not points:
            continue
        pts = " ".join(f"{x:.1f},{y:.1f}" for x, y in points)
        dash = dash_patterns[idx % len(dash_patterns)]
        dash_attr = f' stroke-dasharray="{dash}"' if dash else ""
        parts.append(f'<polyline points="{pts}" fill="none" stroke="black" stroke-width="2"{dash_attr}/>')
        for x, y in points:
            parts.append(f'<circle cx="{x:.1f}" cy="{y:.1f}" r="3" fill="black"/>')
        parts.append(f'<text x="{left + 10}" y="{top + 20 + idx*18}" font-family="sans-serif" font-size="12">{label}</text>')
    for r in rows:
        x = sx(float(r["gap_us"]))
        parts.append(f'<text x="{x:.1f}" y="{top+plot_h+22}" text-anchor="middle" font-family="sans-serif" font-size="11">{r["gap_us"]}</text>')
    parts.append("</svg>\n")
    path.write_text("\n".join(parts), encoding="utf-8")


def write_sweep_outputs(results: Sequence[dict], output_dir: Path) -> dict:
    output_dir.mkdir(parents=True, exist_ok=True)
    analysis = analyze_sweep(results)
    marked = set(analysis["revalidate_10000_gaps_us"])
    normalized: list[dict] = []
    for result in results:
        copy = json.loads(json.dumps(result))
        copy["revalidate_10000"] = int(copy["gap_us"]) in marked
        normalized.append(copy)

    document = {"schema_version": SCHEMA_VERSION, "runs": normalized, "analysis": analysis}
    (output_dir / "results.json").write_text(json.dumps(document, indent=2, sort_keys=False), encoding="utf-8")
    with (output_dir / "results.csv").open("w", encoding="utf-8", newline="") as fh:
        writer = csv.DictWriter(fh, fieldnames=RESULT_CSV_FIELDS, lineterminator="\n")
        writer.writeheader()
        for result in normalized:
            writer.writerow(_flatten(result))

    flat = [_flatten(result) for result in normalized]
    loss_rows = [{"gap_us": r["gap_us"], "loss_pct": float(r["loss_rate"]) * 100.0, "crc_bad_pct": float(r["crc_bad_rate"]) * 100.0} for r in flat]
    _svg_chart(output_dir / "loss_transition.svg", loss_rows, [("loss_pct", "missing / packet span (%)"), ("crc_bad_pct", "CRC-bad / packet span (%)")], "PR1 loss transition sweep", "percent")
    timing_keys = [
        ("irq_to_spi_us_p99", "IRQ to SPI p99"),
        ("spi_duration_us_p99", "SPI duration p99"),
        ("spi_end_to_rearm_start_us_p99", "SPI end to rearm start p99"),
        ("rx_rearm_us_p99", "RX rearm p99"),
        ("irq_to_rx_ready_us_p99", "IRQ to RX ready p99"),
    ]
    _svg_chart(output_dir / "timing_p99.svg", flat, timing_keys, "PR1 RX timing p99", "microseconds")
    return document


def create_plan(output_dir: Path, firmware_sha: str, target_packets: int = 1000) -> list[dict]:
    output_dir.mkdir(parents=True, exist_ok=True)
    manifest: list[dict] = []
    for index, gap in enumerate(SWEEP_GAPS_US, start=1):
        run_id = f"{index:02d}-gap-{gap}us"
        meta = build_run_metadata(gap, target_packets=target_packets, phase="baseline", firmware_sha=firmware_sha, run_id=run_id)
        run_dir = output_dir / "runs" / run_id
        run_dir.mkdir(parents=True, exist_ok=True)
        (run_dir / "metadata.json").write_text(json.dumps(meta, indent=2), encoding="utf-8")
        manifest.append(meta)
    (output_dir / "manifest.json").write_text(json.dumps({"schema_version": SCHEMA_VERSION, "runs": manifest}, indent=2), encoding="utf-8")
    return manifest


def analyze_directory(root: Path) -> dict:
    results: list[dict] = []
    run_root = root / "runs"
    for metadata_path in sorted(run_root.glob("*/metadata.json")):
        run_dir = metadata_path.parent
        meta = json.loads(metadata_path.read_text(encoding="utf-8"))
        rx_path = run_dir / meta.get("files", {}).get("rx_log", "rx.log")
        tx_path = run_dir / meta.get("files", {}).get("tx_log", "tx.log")
        if not rx_path.exists():
            continue
        rx_lines = rx_path.read_text(encoding="utf-8", errors="replace").splitlines()
        tx_lines = tx_path.read_text(encoding="utf-8", errors="replace").splitlines() if tx_path.exists() else None
        result = parse_run_logs(meta, rx_lines, tx_lines)
        (run_dir / "result.json").write_text(json.dumps(result, indent=2), encoding="utf-8")
        results.append(result)
    if not results:
        raise ValueError(f"no completed runs found below {run_root}")
    return write_sweep_outputs(results, root / "report")


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest="command", required=True)

    plan = sub.add_parser("plan", help="create the Monday sweep run directories and metadata")
    plan.add_argument("output_dir", type=Path)
    plan.add_argument("--firmware-sha", required=True)
    plan.add_argument("--target-packets", type=int, default=1000)

    analyze = sub.add_parser("analyze", help="parse completed run logs and write CSV/JSON/SVG reports")
    analyze.add_argument("output_dir", type=Path)

    flash_tx = sub.add_parser("flash-tx", help="flash one TX sweep gap using an untracked generated PlatformIO config")
    flash_tx.add_argument("project_dir", type=Path)
    flash_tx.add_argument("--gap-us", type=int, required=True, choices=SWEEP_GAPS_US)
    flash_tx.add_argument("--port", required=True)
    flash_tx.add_argument("--config-out", type=Path, required=True)
    flash_tx.add_argument("--dry-run", action="store_true")

    args = parser.parse_args(argv)
    try:
        if args.command == "plan":
            runs = create_plan(args.output_dir, args.firmware_sha, args.target_packets)
            print(json.dumps({"runs": len(runs), "gaps_us": SWEEP_GAPS_US}))
        elif args.command == "analyze":
            doc = analyze_directory(args.output_dir)
            print(json.dumps(doc["analysis"], indent=2))
        else:
            base_path = args.project_dir / "platformio.ini"
            base_text = base_path.read_text(encoding="utf-8")
            rendered = render_tx_sweep_config(base_text, args.gap_us)
            args.config_out.parent.mkdir(parents=True, exist_ok=True)
            args.config_out.write_text(rendered, encoding="utf-8")
            command = build_flash_command(
                project_dir=args.project_dir,
                config_path=args.config_out,
                environment="rf_tx_compile",
                port=args.port,
            )
            print(" ".join(shlex.quote(part) for part in command))
            if not args.dry_run:
                completed = subprocess.run(command, check=False)
                if completed.returncode != 0:
                    return completed.returncode
    except (OSError, ValueError, KeyError, json.JSONDecodeError) as exc:
        print(f"pr1_board_sweep: {exc}")
        return 2
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
