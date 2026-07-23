"""Unit tests for the PR1 RF application protocol.

Run from repository root:
    python -m unittest tools.test_pr1_rf_protocol -v
"""

import unittest

from tools.pr1_rf_protocol import (
    HEADER_SIZE,
    Reassembler,
    fragment_frame,
    pack_header,
    unpack_header,
)


class HeaderTests(unittest.TestCase):
    def test_header_is_ten_bytes(self) -> None:
        self.assertEqual(HEADER_SIZE, 10)

    def test_header_round_trip(self) -> None:
        raw = pack_header(
            flags=0x03,
            packet_seq=65535,
            frame_seq=1234,
            fragment_index=2,
            fragment_count=4,
        )
        parsed = unpack_header(raw)
        self.assertEqual(parsed.flags, 0x03)
        self.assertEqual(parsed.packet_seq, 65535)
        self.assertEqual(parsed.frame_seq, 1234)
        self.assertEqual(parsed.fragment_index, 2)
        self.assertEqual(parsed.fragment_count, 4)

    def test_bad_fragment_fields_fail(self) -> None:
        with self.assertRaises(ValueError):
            pack_header(
                flags=0,
                packet_seq=0,
                frame_seq=0,
                fragment_index=1,
                fragment_count=1,
            )


class FragmentationTests(unittest.TestCase):
    def test_32khz_pcm_10ms_needs_six_flrc_packets(self) -> None:
        # 32 kHz * 16 bit * mono * 10 ms = 640 bytes.
        packets = fragment_frame(
            bytes(640), max_rf_payload=127, frame_seq=1
        )
        self.assertEqual(len(packets), 6)
        self.assertTrue(all(len(packet) <= 127 for packet in packets))

    def test_32khz_pcm_10ms_needs_three_gfsk_packets(self) -> None:
        packets = fragment_frame(
            bytes(640), max_rf_payload=255, frame_seq=1
        )
        self.assertEqual(len(packets), 3)
        self.assertTrue(all(len(packet) <= 255 for packet in packets))

    def test_84_byte_codec_frame_fits_one_flrc_packet(self) -> None:
        # Approximate 16 kHz mono IMA-ADPCM candidate:
        # 80 B coded audio + ~4 B block state per 10 ms.
        packets = fragment_frame(
            bytes(84), max_rf_payload=127, frame_seq=1
        )
        self.assertEqual(len(packets), 1)
        self.assertEqual(len(packets[0]), 94)

    def test_out_of_order_reassembly(self) -> None:
        original = bytes(range(256)) * 2 + bytes(range(128))
        packets = fragment_frame(
            original,
            max_rf_payload=127,
            frame_seq=42,
            packet_seq_start=100,
        )

        reassembler = Reassembler()
        result = None
        for packet in reversed(packets):
            completed = reassembler.push(packet)
            if completed is not None:
                result = completed

        self.assertIsNotNone(result)
        assert result is not None
        frame_seq, reconstructed = result
        self.assertEqual(frame_seq, 42)
        self.assertEqual(reconstructed, original)

    def test_duplicate_fragment_is_ignored(self) -> None:
        original = bytes(range(200))
        packets = fragment_frame(
            original, max_rf_payload=127, frame_seq=7
        )
        self.assertEqual(len(packets), 2)

        reassembler = Reassembler()
        self.assertIsNone(reassembler.push(packets[0]))
        self.assertIsNone(reassembler.push(packets[0]))
        completed = reassembler.push(packets[1])
        self.assertEqual(completed, (7, original))


if __name__ == "__main__":
    unittest.main()
