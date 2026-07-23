#!/usr/bin/env python3
"""Summarize PR1 RF/audio field measurements from data/rf_test_log.csv."""

from __future__ import annotations

import argparse
from collections import defaultdict
import csv
from pathlib import Path
import statistics
import sys


def clean(value: object) -> str:
    return str(value or "").strip()


def number(value: object) -> float | None:
    raw = clean(value).replace(",", "")
    if not raw:
        return None
    try:
        return float(raw)
    except ValueError:
        return None


def derived_loss(row: dict[str, str]) -> float | None:
    explicit = number(row.get("loss_pct"))
    if explicit is not None:
        return explicit
    tx = number(row.get("tx_packets"))
    rx = number(row.get("rx_packets"))
    if tx is None or rx is None or tx <= 0:
        return None
    return max(0.0, (tx - rx) / tx * 100.0)


def derived_phrase_accuracy(row: dict[str, str]) -> float | None:
    explicit = number(row.get("phrase_accuracy"))
    if explicit is not None:
        # Accept either 0-1 or 0-100 input.
        return explicit * 100.0 if 0.0 <= explicit <= 1.0 else explicit
    total = number(row.get("phrase_total"))
    correct = number(row.get("phrase_correct"))
    if total is None or correct is None or total <= 0:
        return None
    return correct / total * 100.0


def mean_or_none(values: list[float]) -> float | None:
    return statistics.fmean(values) if values else None


def fmt(value: float | None, suffix: str = "") -> str:
    return "-" if value is None else f"{value:.1f}{suffix}"


def analyze(path: Path) -> int:
    if not path.exists():
        raise FileNotFoundError(path)

    with path.open("r", encoding="utf-8-sig", newline="") as handle:
        rows = [row for row in csv.DictReader(handle) if clean(row.get("run_id"))]

    print(f"PR1 RF field runs: {len(rows)}")
    if not rows:
        print("  No completed runs yet.")
        return 0

    grouped: dict[tuple[str, str], list[dict[str, str]]] = defaultdict(list)
    for row in rows:
        distance = clean(row.get("distance_m")) or "?"
        mode = clean(row.get("mode")) or "?"
        grouped[(distance, mode)].append(row)

    print("\nBy distance / mode")
    print("distance_m | mode | runs | loss_avg | phrase_accuracy | latency_p50")
    print("-----------|------|------|----------|-----------------|------------")

    for (distance, mode), group in sorted(
        grouped.items(), key=lambda item: (number(item[0][0]) or 1e9, item[0][1])
    ):
        losses = [v for row in group if (v := derived_loss(row)) is not None]
        accuracies = [
            v for row in group if (v := derived_phrase_accuracy(row)) is not None
        ]
        latencies = [
            v for row in group if (v := number(row.get("latency_p50_ms"))) is not None
        ]
        print(
            f"{distance:>10} | {mode:<12} | {len(group):>4} | "
            f"{fmt(mean_or_none(losses), '%'):>8} | "
            f"{fmt(mean_or_none(accuracies), '%'):>15} | "
            f"{fmt(mean_or_none(latencies), ' ms'):>10}"
        )

    # Internal MVP checkpoints, only when matching measurements exist.
    for target_distance, target_accuracy in ((20.0, 95.0), (50.0, 90.0)):
        matching = []
        for row in rows:
            distance = number(row.get("distance_m"))
            accuracy = derived_phrase_accuracy(row)
            if distance is not None and abs(distance - target_distance) < 0.01 and accuracy is not None:
                matching.append(accuracy)
        if matching:
            avg = statistics.fmean(matching)
            print(
                f"\n{int(target_distance)}m phrase gate: {avg:.1f}% "
                f"(target >= {target_accuracy:.0f}%) -> "
                f"{'PASS' if avg >= target_accuracy else 'FAIL'}"
            )

    return 0


def parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "csv_path",
        nargs="?",
        default="data/rf_test_log.csv",
        help="RF test CSV (default: data/rf_test_log.csv)",
    )
    return parser.parse_args(argv)


def main(argv: list[str] | None = None) -> int:
    args = parse_args(sys.argv[1:] if argv is None else argv)
    try:
        return analyze(Path(args.csv_path))
    except (OSError, csv.Error) as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
