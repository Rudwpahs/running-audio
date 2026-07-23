from __future__ import annotations

import pathlib
import sys
import unittest

import numpy as np

HOST_DIR = pathlib.Path(__file__).resolve().parents[1] / "host"
sys.path.insert(0, str(HOST_DIR))

import pc_audio_sender  # noqa: E402


class HostSignalTest(unittest.TestCase):
    def test_streaming_decimator_keeps_fixed_frame_size(self) -> None:
        decimator = pc_audio_sender.StreamingDecimator()
        source = np.zeros((480, 1), dtype=np.float32)

        first = decimator.convert(source, 0.10)
        second = decimator.convert(source, 0.10)

        self.assertEqual(first.shape, (160,))
        self.assertEqual(second.shape, (160,))
        self.assertEqual(first.dtype.str, "<i2")
        self.assertTrue(np.all(first == 0))
        self.assertTrue(np.all(second == 0))

    def test_test_tone_is_low_level_pcm16(self) -> None:
        first = pc_audio_sender.make_tone_frame(0, 440.0, 0.10)
        second = pc_audio_sender.make_tone_frame(160, 440.0, 0.10)

        self.assertEqual(first.shape, (160,))
        self.assertEqual(second.shape, (160,))
        self.assertEqual(first.dtype.str, "<i2")
        self.assertLessEqual(int(np.max(np.abs(first))), 3277)
        self.assertLessEqual(int(np.max(np.abs(second))), 3277)
        self.assertFalse(np.array_equal(first, second))


if __name__ == "__main__":
    unittest.main()
