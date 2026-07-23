"""Analyze PR1 M0 serial logs.

Usage:
    python tools/analyze_m0_serial.py rx.log

The analyzer treats the first valid rx_ok packet as the start of the observation
window. It cannot infer packets lost before the first observed packet or after
the final observed packet. For a controlled run, compare with the TX log using
`analyze_m0_run.py`.
"""

from __future__ import annotations

import argparse
import json
import statistics
from dataclasses import asdict, dataclass
from pathlib import Path
from typing import Iterable, Optional


@dataclass
class Summary:
    rx_ok_lines: int = 0
    sequence_missing_inside_window: int = 0
    duplicates: int = 0
    reordered_or_old: int = 0
    bad_header_lines: int = 0
    bad_pattern_lines: int = 0
    radio_error_lines: int = 0
    first_packet_seq: Optional[int] = None
    latest_forward_packet_seq: Optional[int] = None
    rssi_avg_dbm: Optional[float] = None
    rssi_min_dbm: Optional[float] = None
    snr_avg_db: Optional[float] = None
    snr_min_db: Optional[float] = None


def parse_pr1_line(line: str) -> Optional[tuple[str, dict[str, str]]]:
    parts = line.strip().split(",")
    if len(parts) < 2 or parts[0] != "PR1":
        return None

    event = parts[1]
    fields: dict[str, str] = {}
    for part in parts[2:]:
        if "=" not in part:
            continue
        key, value = part.split("=", 1)
        fields[key] = value
    return event, fields


def _float_field(fields: dict[str, str], key: str) -> Optional[float]:
    value = fields.get(key)
    if value is None:
        return None
    try:
        return float(value)
    except ValueError:
        return None


def analyze_lines(lines: Iterable[str]) -> Summary:
    summary = Summary()
    latest_forward_seq: Optional[int] = None
    rssi_values: list[float] = []
    snr_values: list[float] = []

    for line in lines:
        parsed = parse_pr1_line(line)
        if parsed is None:
            continue
        event, fields = parsed

        if event == "rx_bad_header":
            summary.bad_header_lines += 1
            continue
        if event == "rx_bad_pattern":
            summary.bad_pattern_lines += 1
            continue
        if event == "rx_radio_error":
            summary.radio_error_lines += 1
            continue
        if event != "rx_ok":
            continue

        raw_seq = fields.get("packet_seq")
        if raw_seq is None:
            continue
        try:
            packet_seq = int(raw_seq)
        except ValueError:
            continue
        if not 0 <= packet_seq <= 0xFFFF:
            continue

        summary.rx_ok_lines += 1
        rssi = _float_field(fields, "rssi")
        snr = _float_field(fields, "snr")
        if rssi is not None:
            rssi_values.append(rssi)
        if snr is not None:
            snr_values.append(snr)

        if latest_forward_seq is None:
            summary.first_packet_seq = packet_seq
            latest_forward_seq = packet_seq
            continue

        delta = (packet_seq - latest_forward_seq) & 0xFFFF

        if delta == 0:
            summary.duplicates += 1
        elif delta < 0x8000:
            if delta > 1:
                summary.sequence_missing_inside_window += delta - 1
            latest_forward_seq = packet_seq
        else:
            summary.reordered_or_old += 1

    summary.latest_forward_packet_seq = latest_forward_seq

    if rssi_values:
        summary.rssi_avg_dbm = statistics.fmean(rssi_values)
        summary.rssi_min_dbm = min(rssi_values)
    if snr_values:
        summary.snr_avg_db = statistics.fmean(snr_values)
        summary.snr_min_db = min(snr_values)

    return summary


def print_human(summary: Summary) -> None:
    observed = summary.rx_ok_lines + summary.sequence_missing_inside_window
    internal_loss_pct = (
        100.0 * summary.sequence_missing_inside_window / observed
        if observed > 0
        else 0.0
    )

    print("PR1 M0 RX summary")
    print(f"  valid rx_ok lines:              {summary.rx_ok_lines}")
    print(
        "  missing inside seq window:      "
        f"{summary.sequence_missing_inside_window}"
    )
    print(f"  internal sequence loss:          {internal_loss_pct:.3f}%")
    print(f"  duplicates:                      {summary.duplicates}")
    print(f"  reordered/old:                   {summary.reordered_or_old}")
    print(f"  bad header lines:                {summary.bad_header_lines}")
    print(f"  bad payload pattern lines:       {summary.bad_pattern_lines}")
    print(f"  radio error lines:               {summary.radio_error_lines}")
    print(f"  first packet seq:                {summary.first_packet_seq}")
    print(
        "  latest forward packet seq:       "
        f"{summary.latest_forward_packet_seq}"
    )

    if summary.rssi_avg_dbm is not None:
        print(
            "  RSSI avg/min:                    "
            f"{summary.rssi_avg_dbm:.2f} / {summary.rssi_min_dbm:.2f} dBm"
        )
    if summary.snr_avg_db is not None:
        print(
            "  SNR avg/min:                     "
            f"{summary.snr_avg_db:.2f} / {summary.snr_min_db:.2f} dB"
        )

    print()
    print(
        "Note: sequence loss only covers gaps between the first and latest "
        "valid RX sequence. Use the TX sent count for the final packet-loss "
        "rate of a controlled run."
    )


def main() -> None:
    parser = argparse.ArgumentParser(description="Analyze PR1 M0 receiver logs")
    parser.add_argument("logfile", type=Path)
    parser.add_argument(
        "--json",
        action="store_true",
        help="print machine-readable JSON instead of the human summary",
    )
    args = parser.parse_args()

    with args.logfile.open("r", encoding="utf-8", errors="replace") as handle:
        summary = analyze_lines(handle)

    if args.json:
        print(json.dumps(asdict(summary), ensure_ascii=False, indent=2))
    else:
        print_human(summary)


if __name__ == "__main__":
    main()
