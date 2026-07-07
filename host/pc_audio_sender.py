#!/usr/bin/env python3
"""Capture PC playback and stream PR1 v2 PCM frames over USB serial."""

from __future__ import annotations

import argparse
import struct
import sys
import time
from dataclasses import dataclass

import numpy as np
import serial
import soundcard as sc
from scipy.signal import resample_poly

SOURCE_RATE = 48_000
TARGET_RATE = 16_000
SOURCE_FRAMES = 360
TARGET_FRAMES = 120
SERIAL_MAGIC = b"PR1U"
SERIAL_BAUD = 921_600


@dataclass(frozen=True)
class SenderConfig:
    port: str
    volume: float
    device_name: str | None


def list_capture_devices() -> None:
    print("Speakers:")
    for speaker in sc.all_speakers():
        print(f"  {speaker.name}")
    print("\nMicrophones and loopback endpoints:")
    for microphone in sc.all_microphones(include_loopback=True):
        print(f"  {microphone.name}")


def select_loopback(device_name: str | None):
    if device_name:
        matches = [
            item
            for item in sc.all_microphones(include_loopback=True)
            if device_name.casefold() in item.name.casefold()
        ]
        if not matches:
            raise RuntimeError(
                f"No loopback/capture device contains {device_name!r}. "
                "Run with --list-devices."
            )
        return matches[0]

    speaker = sc.default_speaker()
    if speaker is None:
        raise RuntimeError("No default speaker was found.")
    return sc.get_microphone(speaker.name, include_loopback=True)


def float_to_pcm16(block: np.ndarray, volume: float) -> np.ndarray:
    mono = np.asarray(block, dtype=np.float32)
    if mono.ndim == 2:
        mono = mono.mean(axis=1)
    mono = resample_poly(mono, up=1, down=3)
    if mono.shape[0] < TARGET_FRAMES:
        mono = np.pad(mono, (0, TARGET_FRAMES - mono.shape[0]))
    mono = mono[:TARGET_FRAMES]
    mono = np.clip(mono * volume, -1.0, 1.0)
    return np.rint(mono * 32767.0).astype("<i2")


def drain_status(transport: serial.Serial) -> None:
    while transport.in_waiting:
        line = transport.readline().decode("utf-8", errors="replace").strip()
        if line:
            print(f"\n{line}")


def run_sender(config: SenderConfig) -> None:
    capture = select_loopback(config.device_name)
    print(f"Capture device: {capture.name}")
    print(f"Serial port: {config.port} @ {SERIAL_BAUD}")
    print("Press Ctrl+C to stop.")

    sequence = 0
    sent_frames = 0
    report_started = time.monotonic()

    with serial.Serial(
        config.port,
        SERIAL_BAUD,
        timeout=0.05,
        write_timeout=1.0,
    ) as transport:
        time.sleep(1.5)
        transport.reset_input_buffer()
        transport.reset_output_buffer()

        with capture.recorder(
            samplerate=SOURCE_RATE,
            channels=1,
            blocksize=SOURCE_FRAMES * 4,
        ) as recorder:
            while True:
                source = recorder.record(numframes=SOURCE_FRAMES)
                pcm = float_to_pcm16(source, config.volume)
                frame = SERIAL_MAGIC + struct.pack("<I", sequence) + pcm.tobytes()
                transport.write(frame)
                sequence = (sequence + 1) & 0xFFFFFFFF
                sent_frames += 1
                drain_status(transport)

                now = time.monotonic()
                if now - report_started >= 1.0:
                    print(
                        f"\rframes={sent_frames} "
                        f"audio_seconds={sent_frames * 0.0075:.1f}",
                        end="",
                        flush=True,
                    )
                    report_started = now


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Stream PC playback to the PR1 SX1280 transmitter."
    )
    parser.add_argument("--port", help="Transmitter port, for example COM7")
    parser.add_argument(
        "--volume",
        type=float,
        default=0.5,
        help="Input scaling from 0.0 to 1.0; the receiver also limits output.",
    )
    parser.add_argument(
        "--device",
        help="Substring of a loopback/capture device name.",
    )
    parser.add_argument(
        "--list-devices",
        action="store_true",
        help="List available devices and exit.",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    if args.list_devices:
        list_capture_devices()
        return 0
    if not args.port:
        print("--port is required unless --list-devices is used.", file=sys.stderr)
        return 2
    if not 0.0 <= args.volume <= 1.0:
        print("--volume must be between 0.0 and 1.0.", file=sys.stderr)
        return 2

    try:
        run_sender(SenderConfig(args.port, args.volume, args.device))
    except KeyboardInterrupt:
        print("\nStopped.")
        return 0
    except Exception as exc:
        print(f"\nError: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
