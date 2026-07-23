"""Compare one controlled PR1 M0 transmitter/receiver run.

Usage:
    python tools/analyze_m0_run.py tx.log rx.log
    python tools/analyze_m0_run.py tx.log rx.log --json

For the current M0 gate, keep a single run below 65,536 successful TX packets
so the uint16 packet sequence number is unique within that run. The planned
10,000-packet test is safely inside that limit.
"""

from __future__ import annotations

import argparse
import json
from dataclasses import asdict, dataclass
from pathlib import Path
from typing import Iterable

from tools.analyze_m0_serial import parse_pr1_line


@dataclass
class RunSummary:
    tx_ok_lines: int
    tx_fail_lines: int
    tx_unique_success_sequences: int
    rx_ok_lines: int
    rx_unique_valid_sequences: int
    matched_valid_sequences: int
    missing_after_successful_tx: int
    unexpected_valid_rx_sequences: int
    duplicate_valid_rx_lines: int
    rx_bad_header_lines: int
    rx_bad_pattern_lines: int
    rx_radio_error_lines: int
    packet_loss_pct: float


def _collect_tx(lines: Iterable[str]) -> tuple[list[int], int]:
    tx_ok: list[int] = []
    tx_fail = 0

    for line in lines:
        parsed = parse_pr1_line(line)
        if parsed is None:
            continue
        event, fields = parsed

        if event == "tx_fail":
            tx_fail += 1
            continue
        if event != "tx_ok":
            continue

        try:
            seq = int(fields["packet_seq"])
        except (KeyError, ValueError):
            continue
        if 0 <= seq <= 0xFFFF:
            tx_ok.append(seq)

    return tx_ok, tx_fail


def _collect_rx(
    lines: Iterable[str],
) -> tuple[list[int], int, int, int]:
    rx_ok: list[int] = []
    bad_header = 0
    bad_pattern = 0
    radio_error = 0

    for line in lines:
        parsed = parse_pr1_line(line)
        if parsed is None:
            continue
        event, fields = parsed

        if event == "rx_bad_header":
            bad_header += 1
            continue
        if event == "rx_bad_pattern":
            bad_pattern += 1
            continue
        if event == "rx_radio_error":
            radio_error += 1
            continue
        if event != "rx_ok":
            continue

        try:
            seq = int(fields["packet_seq"])
        except (KeyError, ValueError):
            continue
        if 0 <= seq <= 0xFFFF:
            rx_ok.append(seq)

    return rx_ok, bad_header, bad_pattern, radio_error


def analyze_run(tx_lines: Iterable[str], rx_lines: Iterable[str]) -> RunSummary:
    tx_ok, tx_fail = _collect_tx(tx_lines)
    rx_ok, bad_header, bad_pattern, radio_error = _collect_rx(rx_lines)

    tx_set = set(tx_ok)
    rx_set = set(rx_ok)

    if len(tx_ok) > 0x10000:
        raise ValueError(
            "M0 run has more than 65,536 successful TX packets; packet_seq is "
            "uint16, so split the test into smaller runs before set comparison"
        )

    if len(tx_set) != len(tx_ok):
        raise ValueError(
            "successful TX packet_seq values repeat inside this run; split the "
            "run so each uint16 sequence value is unique"
        )

    matched = tx_set & rx_set
    missing = tx_set - rx_set
    unexpected = rx_set - tx_set
    duplicate_rx = len(rx_ok) - len(rx_set)

    loss_pct = 100.0 * len(missing) / len(tx_set) if tx_set else 0.0

    return RunSummary(
        tx_ok_lines=len(tx_ok),
        tx_fail_lines=tx_fail,
        tx_unique_success_sequences=len(tx_set),
        rx_ok_lines=len(rx_ok),
        rx_unique_valid_sequences=len(rx_set),
        matched_valid_sequences=len(matched),
        missing_after_successful_tx=len(missing),
        unexpected_valid_rx_sequences=len(unexpected),
        duplicate_valid_rx_lines=duplicate_rx,
        rx_bad_header_lines=bad_header,
        rx_bad_pattern_lines=bad_pattern,
        rx_radio_error_lines=radio_error,
        packet_loss_pct=loss_pct,
    )


def print_human(summary: RunSummary) -> None:
    print("PR1 M0 controlled run summary")
    print(f"  TX successful packets:           {summary.tx_ok_lines}")
    print(f"  TX radio failures:               {summary.tx_fail_lines}")
    print(f"  RX valid packets:                {summary.rx_ok_lines}")
    print(f"  matched valid packets:           {summary.matched_valid_sequences}")
    print(
        "  missing after successful TX:     "
        f"{summary.missing_after_successful_tx}"
    )
    print(f"  packet loss:                     {summary.packet_loss_pct:.3f}%")
    print(
        "  unexpected valid RX sequences:   "
        f"{summary.unexpected_valid_rx_sequences}"
    )
    print(f"  duplicate valid RX lines:        {summary.duplicate_valid_rx_lines}")
    print(f"  RX bad header:                   {summary.rx_bad_header_lines}")
    print(f"  RX bad payload pattern:          {summary.rx_bad_pattern_lines}")
    print(f"  RX radio errors:                 {summary.rx_radio_error_lines}")


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Compare PR1 M0 TX/RX logs from one controlled run"
    )
    parser.add_argument("tx_log", type=Path)
    parser.add_argument("rx_log", type=Path)
    parser.add_argument("--json", action="store_true")
    args = parser.parse_args()

    with args.tx_log.open("r", encoding="utf-8", errors="replace") as handle:
        tx_lines = list(handle)
    with args.rx_log.open("r", encoding="utf-8", errors="replace") as handle:
        rx_lines = list(handle)

    summary = analyze_run(tx_lines, rx_lines)
    if args.json:
        print(json.dumps(asdict(summary), ensure_ascii=False, indent=2))
    else:
        print_human(summary)


if __name__ == "__main__":
    main()
