#!/usr/bin/env python3
"""Summarize PR1 buyer interviews against the current validation gate.

The script does not decide whether PR1 is a good business. It only reports
whether the minimum evidence threshold recorded in MARKET_VALIDATION.md has
been reached.
"""

from __future__ import annotations

import argparse
import csv
from pathlib import Path
import sys

EMPTY_VALUES = {"", "0", "0.0", "none", "n/a", "na", "없음", "없다", "무료"}
PAID_LEVELS = {"PAID_PILOT", "PAID", "CONTRACT", "PAYMENT"}


def text(value: object) -> str:
    return str(value or "").strip()


def to_float(value: object) -> float | None:
    raw = text(value).replace(",", "")
    if not raw:
        return None
    try:
        return float(raw)
    except ValueError:
        return None


def has_existing_cost_or_effort(value: object) -> bool:
    raw = text(value).lower()
    return raw not in EMPTY_VALUES


def is_paid_commitment(row: dict[str, str]) -> bool:
    evidence = text(row.get("evidence_level")).upper()
    if evidence in PAID_LEVELS:
        return True

    price = to_float(row.get("pilot_price_krw"))
    pilot_date = text(row.get("pilot_date"))
    return price is not None and price > 0 and bool(pilot_date)


def analyze(path: Path) -> dict[str, int | bool]:
    if not path.exists():
        raise FileNotFoundError(path)

    with path.open("r", encoding="utf-8-sig", newline="") as handle:
        rows = [row for row in csv.DictReader(handle) if text(row.get("organization"))]

    repeated_problem = 0
    current_spend_or_effort = 0
    paid_pilot = 0

    for row in rows:
        frequency = to_float(row.get("frequency_per_month"))
        latest_incident = text(row.get("latest_incident"))
        if frequency is not None and frequency >= 2 and latest_incident:
            repeated_problem += 1

        if has_existing_cost_or_effort(row.get("current_cost_or_effort")):
            current_spend_or_effort += 1

        if is_paid_commitment(row):
            paid_pilot += 1

    total = len(rows)
    gate_met = (
        total >= 10
        and repeated_problem >= 5
        and current_spend_or_effort >= 3
        and paid_pilot >= 2
    )

    return {
        "interviews": total,
        "repeated_problem": repeated_problem,
        "current_spend_or_effort": current_spend_or_effort,
        "paid_pilot": paid_pilot,
        "gate_met": gate_met,
    }


def parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "csv_path",
        nargs="?",
        default="data/interview_log.csv",
        help="buyer interview CSV (default: data/interview_log.csv)",
    )
    return parser.parse_args(argv)


def main(argv: list[str] | None = None) -> int:
    args = parse_args(sys.argv[1:] if argv is None else argv)
    try:
        result = analyze(Path(args.csv_path))
    except (OSError, csv.Error) as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        return 2

    print("PR1 buyer-validation evidence")
    print(f"  completed interviews:      {result['interviews']} / 10")
    print(f"  repeated problems:         {result['repeated_problem']} / 5")
    print(f"  current spend/effort:      {result['current_spend_or_effort']} / 3")
    print(f"  paid pilot commitments:    {result['paid_pilot']} / 2")
    print(f"  minimum GO evidence gate:  {'MET' if result['gate_met'] else 'NOT MET'}")

    # A not-yet-met business gate is not a software failure. The process should
    # still exit 0 so it can be run on an empty/new data file in CI.
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
