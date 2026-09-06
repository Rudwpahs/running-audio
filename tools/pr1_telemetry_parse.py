#!/usr/bin/env python3
"""Parse PR1 serial telemetry/event records into JSONL or CSV."""

from __future__ import annotations

import argparse
import csv
import json
import sys
from pathlib import Path
from typing import Iterable, TextIO


CSV_FIELDS = ["kind", "version", "t_us", "seq", "field", "event", "value"]


def _parse_decimal(values: dict[str, str], key: str) -> int:
    text = values.get(key)
    if text is None or text == "":
        raise ValueError(f"missing required key: {key}")
    try:
        return int(text, 10)
    except ValueError as exc:
        raise ValueError(f"invalid integer for {key}: {text}") from exc


def _required_text(values: dict[str, str], key: str) -> str:
    text = values.get(key)
    if text is None or text == "":
        raise ValueError(f"missing required key: {key}")
    return text


def parse_line(line: str) -> dict[str, int | str] | None:
    """Parse one PR1T/PR1E record; return None for unrelated serial output."""
    line = line.strip()
    if not line.startswith(("PR1T ", "PR1E ")):
        return None

    tokens = line.split()
    prefix = tokens[0]
    values: dict[str, str] = {}
    for token in tokens[1:]:
        if "=" not in token:
            raise ValueError(f"malformed token: {token}")
        key, value = token.split("=", 1)
        if key == "":
            raise ValueError(f"malformed token: {token}")
        values[key] = value

    version = _parse_decimal(values, "v")
    timestamp_us = _parse_decimal(values, "t_us")
    value = _parse_decimal(values, "value")

    if prefix == "PR1T":
        return {
            "kind": "telemetry",
            "version": version,
            "t_us": timestamp_us,
            "field": _required_text(values, "field"),
            "value": value,
        }

    return {
        "kind": "event",
        "version": version,
        "t_us": timestamp_us,
        "seq": _parse_decimal(values, "seq"),
        "event": _required_text(values, "event"),
        "value": value,
    }


def _iter_records(lines: Iterable[str]):
    for line_number, line in enumerate(lines, start=1):
        try:
            record = parse_line(line)
        except ValueError as exc:
            raise ValueError(f"line {line_number}: {exc}") from exc
        if record is not None:
            yield record


def _write_jsonl(records: Iterable[dict[str, int | str]], output: TextIO) -> None:
    for record in records:
        output.write(json.dumps(record, separators=(",", ":"), sort_keys=False))
        output.write("\n")


def _write_csv(records: Iterable[dict[str, int | str]], output: TextIO) -> None:
    writer = csv.DictWriter(output, fieldnames=CSV_FIELDS, extrasaction="ignore", lineterminator="\n")
    writer.writeheader()
    for record in records:
        writer.writerow(record)


def _open_input(path: str | None):
    if path is None:
        return sys.stdin, False
    return Path(path).open("r", encoding="utf-8"), True


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("input", nargs="?", help="serial log file; stdin when omitted")
    parser.add_argument("--format", choices=("jsonl", "csv"), default="jsonl")
    args = parser.parse_args(argv)

    stream, should_close = _open_input(args.input)
    try:
        records = _iter_records(stream)
        if args.format == "csv":
            _write_csv(records, sys.stdout)
        else:
            _write_jsonl(records, sys.stdout)
    except (OSError, ValueError) as exc:
        print(f"pr1_telemetry_parse: {exc}", file=sys.stderr)
        return 2
    finally:
        if should_close:
            stream.close()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
