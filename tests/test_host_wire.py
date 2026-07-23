from __future__ import annotations

import pathlib
import sys
import unittest

HOST_DIR = pathlib.Path(__file__).resolve().parents[1] / "host"
sys.path.insert(0, str(HOST_DIR))

import pr1_wire  # noqa: E402


class UsbWireTest(unittest.TestCase):
    def test_python_frame_matches_protocol_v3(self) -> None:
        pcm = bytes((index * 17 + 3) & 0xFF for index in range(320))
        frame = pr1_wire.build_usb_frame(0x10203040, 0x89ABCDEF, pcm)

        self.assertEqual(len(frame), 340)
        magic, version, flags, payload_len, stream_id, sequence, crc = (
            pr1_wire.unpack_usb_header(frame)
        )
        self.assertEqual(magic, b"PR1U")
        self.assertEqual(version, 3)
        self.assertEqual(flags, 1)
        self.assertEqual(payload_len, 320)
        self.assertEqual(stream_id, 0x10203040)
        self.assertEqual(sequence, 0x89ABCDEF)
        self.assertEqual(crc, 0xDB4A46EC)
        self.assertEqual(crc, pr1_wire.frame_crc32(pcm))
        self.assertEqual(
            frame[:20],
            bytes.fromhex(
                "50 52 31 55 03 01 40 01 40 30 20 10 "
                "EF CD AB 89 EC 46 4A DB"
            ),
        )
        self.assertEqual(frame[20:], pcm)

    def test_wrong_pcm_length_is_rejected(self) -> None:
        with self.assertRaises(ValueError):
            pr1_wire.build_usb_frame(0, 0, bytes(319))

    def test_out_of_range_ids_are_rejected(self) -> None:
        pcm = bytes(320)
        with self.assertRaises(ValueError):
            pr1_wire.build_usb_frame(0, -1, pcm)
        with self.assertRaises(ValueError):
            pr1_wire.build_usb_frame(0, 0x1_0000_0000, pcm)
        with self.assertRaises(ValueError):
            pr1_wire.build_usb_frame(-1, 0, pcm)
        with self.assertRaises(ValueError):
            pr1_wire.build_usb_frame(0x1_0000_0000, 0, pcm)


if __name__ == "__main__":
    unittest.main()
