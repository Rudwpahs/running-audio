import unittest

from tools.analyze_m0_serial import analyze_lines, parse_pr1_line


class ParserTests(unittest.TestCase):
    def test_ignores_non_pr1_line(self) -> None:
        self.assertIsNone(parse_pr1_line("booting..."))

    def test_parses_fields(self) -> None:
        event, fields = parse_pr1_line(
            "PR1,rx_ok,packet_seq=12,rssi=-71.5,snr=8.0"
        )
        self.assertEqual(event, "rx_ok")
        self.assertEqual(fields["packet_seq"], "12")
        self.assertEqual(fields["rssi"], "-71.5")


class AnalyzeTests(unittest.TestCase):
    def test_gap_duplicate_reorder_and_corruption(self) -> None:
        lines = [
            "PR1,rx_ok,packet_seq=10,frame_seq=10,len=70,rssi=-70.0,snr=8.1\n",
            "PR1,rx_ok,packet_seq=11,frame_seq=11,len=70,rssi=-71.0,snr=7.5\n",
            # Sequence 12 is missing when 13 arrives.
            "PR1,rx_ok,packet_seq=13,frame_seq=13,len=70,rssi=-75.0,snr=6.0\n",
            # Duplicate latest packet.
            "PR1,rx_ok,packet_seq=13,frame_seq=13,len=70,rssi=-75.0,snr=6.0\n",
            # Old/reordered packet 12 arrives after 13.
            "PR1,rx_ok,packet_seq=12,frame_seq=12,len=70,rssi=-74.0,snr=6.2\n",
            "PR1,rx_bad_pattern,packet_seq=14,count=1,rssi=-76.0,snr=5.5\n",
            "PR1,rx_bad_header,count=1,rssi=-80.0,snr=4.0\n",
            "PR1,rx_radio_error,state=-6\n",
        ]

        summary = analyze_lines(lines)
        self.assertEqual(summary.rx_ok_lines, 5)
        self.assertEqual(summary.sequence_missing_inside_window, 1)
        self.assertEqual(summary.duplicates, 1)
        self.assertEqual(summary.reordered_or_old, 1)
        self.assertEqual(summary.bad_pattern_lines, 1)
        self.assertEqual(summary.bad_header_lines, 1)
        self.assertEqual(summary.radio_error_lines, 1)
        self.assertEqual(summary.first_packet_seq, 10)
        self.assertEqual(summary.latest_forward_packet_seq, 13)
        self.assertAlmostEqual(summary.rssi_avg_dbm, -73.0)
        self.assertAlmostEqual(summary.rssi_min_dbm, -75.0)

    def test_uint16_wrap_is_forward_progress(self) -> None:
        lines = [
            "PR1,rx_ok,packet_seq=65534,rssi=-60,snr=10\n",
            "PR1,rx_ok,packet_seq=65535,rssi=-60,snr=10\n",
            "PR1,rx_ok,packet_seq=0,rssi=-60,snr=10\n",
            "PR1,rx_ok,packet_seq=1,rssi=-60,snr=10\n",
        ]

        summary = analyze_lines(lines)
        self.assertEqual(summary.sequence_missing_inside_window, 0)
        self.assertEqual(summary.reordered_or_old, 0)
        self.assertEqual(summary.latest_forward_packet_seq, 1)


if __name__ == "__main__":
    unittest.main()
