import unittest

from tools.analyze_m0_run import analyze_run


class ControlledRunTests(unittest.TestCase):
    def test_true_loss_duplicates_and_corruption(self) -> None:
        tx_lines = [
            "PR1,tx_ok,packet_seq=10,frame_seq=10,len=70\n",
            "PR1,tx_ok,packet_seq=11,frame_seq=11,len=70\n",
            "PR1,tx_ok,packet_seq=12,frame_seq=12,len=70\n",
            "PR1,tx_ok,packet_seq=13,frame_seq=13,len=70\n",
            "PR1,tx_ok,packet_seq=14,frame_seq=14,len=70\n",
            "PR1,tx_fail,packet_seq=15,state=-6\n",
        ]
        rx_lines = [
            "PR1,rx_ok,packet_seq=10,frame_seq=10,len=70\n",
            "PR1,rx_ok,packet_seq=11,frame_seq=11,len=70\n",
            # 12 never arrives.
            "PR1,rx_ok,packet_seq=13,frame_seq=13,len=70\n",
            # Duplicate 13.
            "PR1,rx_ok,packet_seq=13,frame_seq=13,len=70\n",
            # 14 arrives but application pattern is corrupt, so it is not valid.
            "PR1,rx_bad_pattern,packet_seq=14,count=1\n",
            # A stale/unexpected packet from outside the TX observation window.
            "PR1,rx_ok,packet_seq=9,frame_seq=9,len=70\n",
            "PR1,rx_bad_header,count=1\n",
            "PR1,rx_radio_error,state=-6\n",
        ]

        summary = analyze_run(tx_lines, rx_lines)
        self.assertEqual(summary.tx_ok_lines, 5)
        self.assertEqual(summary.tx_fail_lines, 1)
        self.assertEqual(summary.rx_ok_lines, 5)
        self.assertEqual(summary.matched_valid_sequences, 3)
        self.assertEqual(summary.missing_after_successful_tx, 2)
        self.assertEqual(summary.unexpected_valid_rx_sequences, 1)
        self.assertEqual(summary.duplicate_valid_rx_lines, 1)
        self.assertEqual(summary.rx_bad_pattern_lines, 1)
        self.assertEqual(summary.rx_bad_header_lines, 1)
        self.assertEqual(summary.rx_radio_error_lines, 1)
        self.assertAlmostEqual(summary.packet_loss_pct, 40.0)

    def test_zero_loss(self) -> None:
        tx = [
            f"PR1,tx_ok,packet_seq={seq},frame_seq={seq},len=70\n"
            for seq in range(100, 110)
        ]
        rx = [
            f"PR1,rx_ok,packet_seq={seq},frame_seq={seq},len=70\n"
            for seq in range(100, 110)
        ]
        summary = analyze_run(tx, rx)
        self.assertEqual(summary.missing_after_successful_tx, 0)
        self.assertEqual(summary.packet_loss_pct, 0.0)

    def test_repeated_tx_sequence_rejected(self) -> None:
        tx = [
            "PR1,tx_ok,packet_seq=1\n",
            "PR1,tx_ok,packet_seq=1\n",
        ]
        with self.assertRaises(ValueError):
            analyze_run(tx, [])


if __name__ == "__main__":
    unittest.main()
