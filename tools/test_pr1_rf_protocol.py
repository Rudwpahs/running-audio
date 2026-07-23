"""Unit tests for the lower-layer PR1 RF transport protocol."""

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
    def test_m0_90_byte_payload_makes_one_100_byte_rf_packet(self) -> None:
        packets = fragment_frame(
            bytes(90), max_rf_payload=127, frame_seq=1
        )
        self.assertEqual(len(packets), 1)
        self.assertEqual(len(packets[0]), 100)

    def test_voice_v0_176_byte_application_packet_fits_one_255_packet(self) -> None:
        # Canonical application packet on current main:
        # 16-byte app header + 160-byte raw voice payload = 176 bytes.
        packets = fragment_frame(
            bytes(176), max_rf_payload=255, frame_seq=1
        )
        self.assertEqual(len(packets), 1)
        self.assertEqual(len(packets[0]), 186)

    def test_voice_v0_176_byte_application_packet_needs_two_127_packets(self) -> None:
        packets = fragment_frame(
            bytes(176), max_rf_payload=127, frame_seq=1
        )
        self.assertEqual(len(packets), 2)
        self.assertTrue(all(len(packet) <= 127 for packet in packets))

    def test_old_32khz_application_packet_regression_budget(self) -> None:
        # Historical 640-byte raw PCM + current 16-byte application header.
        self.assertEqual(
            len(fragment_frame(bytes(656), max_rf_payload=127, frame_seq=1)),
            6,
        )
        self.assertEqual(
            len(fragment_frame(bytes(656), max_rf_payload=255, frame_seq=1)),
            3,
        )

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

        self.assertEqual(result, (42, original))

    def test_duplicate_fragment_is_ignored(self) -> None:
        original = bytes(range(200))
        packets = fragment_frame(
            original, max_rf_payload=127, frame_seq=7
        )
        reassembler = Reassembler()
        self.assertIsNone(reassembler.push(packets[0]))
        self.assertIsNone(reassembler.push(packets[0]))
        self.assertEqual(reassembler.push(packets[1]), (7, original))

    def test_duplicate_completed_frame_is_ignored(self) -> None:
        original = bytes(range(84))
        packet = fragment_frame(
            original, max_rf_payload=127, frame_seq=99
        )[0]
        reassembler = Reassembler()
        self.assertEqual(reassembler.push(packet), (99, original))
        self.assertIsNone(reassembler.push(packet))

    def test_completed_history_is_bounded(self) -> None:
        reassembler = Reassembler(completed_history=2)
        packets = {
            seq: fragment_frame(
                bytes([seq]), max_rf_payload=127, frame_seq=seq
            )[0]
            for seq in (1, 2, 3)
        }
        self.assertEqual(reassembler.push(packets[1]), (1, b"\x01"))
        self.assertEqual(reassembler.push(packets[2]), (2, b"\x02"))
        self.assertEqual(reassembler.push(packets[3]), (3, b"\x03"))
        self.assertEqual(reassembler.push(packets[1]), (1, b"\x01"))


if __name__ == "__main__":
    unittest.main()
