import csv
import io
import subprocess
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))

from pr1_telemetry_parse import parse_line  # noqa: E402


class TelemetryParserTests(unittest.TestCase):
    def test_parses_telemetry_record(self):
        self.assertEqual(
            parse_line("PR1T v=1 t_us=123 field=crc_bad value=4"),
            {
                "kind": "telemetry",
                "version": 1,
                "t_us": 123,
                "field": "crc_bad",
                "value": 4,
            },
        )

    def test_parses_event_record(self):
        self.assertEqual(
            parse_line("PR1E v=1 t_us=456 seq=9 event=spi_read_start value=0"),
            {
                "kind": "event",
                "version": 1,
                "t_us": 456,
                "seq": 9,
                "event": "spi_read_start",
                "value": 0,
            },
        )

    def test_ignores_unrelated_serial_line(self):
        self.assertIsNone(parse_line("PR1_RUNTIME_BOOT"))

    def test_preserves_unknown_field_and_ignores_extra_keys(self):
        record = parse_line("PR1T v=1 t_us=1 field=future_metric value=7 x=ignored")
        self.assertEqual(record["field"], "future_metric")
        self.assertNotIn("x", record)

    def test_rejects_invalid_integer(self):
        with self.assertRaises(ValueError):
            parse_line("PR1T v=x t_us=1 field=crc_bad value=0")

    def test_rejects_event_without_sequence(self):
        with self.assertRaises(ValueError):
            parse_line("PR1E v=1 t_us=1 event=rx_packet_ok value=0")

    def test_rejects_malformed_pr1_token(self):
        with self.assertRaises(ValueError):
            parse_line("PR1T v=1 t_us=1 field=crc_bad malformed value=0")

    def test_csv_cli_has_stable_header_and_rows(self):
        input_text = (
            "noise before boot\n"
            "PR1T v=1 t_us=123 field=crc_bad value=4\n"
            "PR1E v=1 t_us=456 seq=9 event=spi_read_start value=0\n"
        )
        result = subprocess.run(
            [sys.executable, str(ROOT / "tools" / "pr1_telemetry_parse.py"), "--format", "csv"],
            input=input_text,
            text=True,
            capture_output=True,
            check=False,
        )
        self.assertEqual(result.returncode, 0, result.stderr)
        rows = list(csv.reader(io.StringIO(result.stdout)))
        self.assertEqual(
            rows[0],
            ["kind", "version", "t_us", "seq", "field", "event", "value"],
        )
        self.assertEqual(rows[1], ["telemetry", "1", "123", "", "crc_bad", "", "4"])
        self.assertEqual(rows[2], ["event", "1", "456", "9", "", "spi_read_start", "0"])


if __name__ == "__main__":
    unittest.main()
