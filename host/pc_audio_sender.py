#!/usr/bin/env python3
"""Capture PC playback or generate a test tone for the PR1 v3 transmitter."""

from __future__ import annotations

import argparse
import secrets
import sys
import time
from dataclasses import dataclass

import numpy as np
import serial
from scipy.signal import firwin, lfilter

from pr1_wire import (
    FRAME_MS,
    SAMPLE_RATE,
    SAMPLES_PER_FRAME,
    SERIAL_BAUD,
    build_usb_frame,
)

SOURCE_RATE = 48_000
SOURCE_FRAMES = 480
_STATUS_BUFFER = bytearray()


@dataclass(frozen=True)
class SenderConfig:
    port: str
    stream_id: int
    volume: float
    device_name: str | None
    tone_hz: float | None


def list_capture_devices() -> None:
    import soundcard as sc

    print("Speakers:")
    for speaker in sc.all_speakers():
        print(f"  {speaker.name}")
    print("\nMicrophones and loopback endpoints:")
    for microphone in sc.all_microphones(include_loopback=True):
        print(f"  {microphone.name}")


def select_loopback(device_name: str | None):
    import soundcard as sc

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


class StreamingDecimator:
    """Stateful 48 kHz -> 16 kHz FIR decimator without 10 ms edge resets."""

    def __init__(self) -> None:
        self._taps = firwin(63, 0.30)
        self._state = np.zeros(len(self._taps) - 1, dtype=np.float64)

    def convert(self, block: np.ndarray, volume: float) -> np.ndarray:
        mono = np.asarray(block, dtype=np.float32)
        if mono.ndim == 2:
            mono = mono.mean(axis=1)
        filtered, self._state = lfilter(
            self._taps,
            [1.0],
            mono,
            zi=self._state,
        )
        mono_16k = filtered[::3]
        if mono_16k.shape[0] < SAMPLES_PER_FRAME:
            mono_16k = np.pad(
                mono_16k,
                (0, SAMPLES_PER_FRAME - mono_16k.shape[0]),
            )
        mono_16k = mono_16k[:SAMPLES_PER_FRAME]
        mono_16k = np.clip(mono_16k * volume, -1.0, 1.0)
        return np.rint(mono_16k * 32767.0).astype("<i2")


def make_tone_frame(
    start_sample: int,
    frequency_hz: float,
    volume: float,
) -> np.ndarray:
    positions = start_sample + np.arange(SAMPLES_PER_FRAME)
    waveform = np.sin(2.0 * np.pi * frequency_hz * positions / SAMPLE_RATE)
    waveform = np.clip(waveform * volume, -1.0, 1.0)
    return np.rint(waveform * 32767.0).astype("<i2")


def drain_status(transport: serial.Serial) -> None:
    waiting = transport.in_waiting
    if waiting:
        _STATUS_BUFFER.extend(transport.read(waiting))

    while True:
        newline = _STATUS_BUFFER.find(b"\n")
        if newline < 0:
            break
        raw_line = bytes(_STATUS_BUFFER[:newline])
        del _STATUS_BUFFER[: newline + 1]
        line = raw_line.decode("utf-8", errors="replace").strip()
        if line:
            print(f"\n{line}")


def send_pcm_frame(
    transport: serial.Serial,
    stream_id: int,
    sequence: int,
    pcm: np.ndarray,
) -> None:
    encoded = build_usb_frame(
        stream_id,
        sequence,
        pcm.astype("<i2", copy=False).tobytes(),
    )
    written = transport.write(encoded)
    if written != len(encoded):
        raise RuntimeError(f"Short serial write: {written}/{len(encoded)} bytes")


def report_progress(sent_frames: int, report_started: float) -> float:
    now = time.monotonic()
    if now - report_started >= 1.0:
        audio_seconds = sent_frames * FRAME_MS / 1000.0
        print(
            f"\rframes={sent_frames} audio_seconds={audio_seconds:.1f}",
            end="",
            flush=True,
        )
        return now
    return report_started


def stream_test_tone(
    transport: serial.Serial,
    stream_id: int,
    frequency_hz: float,
    volume: float,
) -> None:
    sequence = 0
    sent_frames = 0
    start_sample = 0
    report_started = time.monotonic()
    deadline = time.monotonic()

    while True:
        pcm = make_tone_frame(start_sample, frequency_hz, volume)
        send_pcm_frame(transport, stream_id, sequence, pcm)
        sequence = (sequence + 1) & 0xFFFF_FFFF
        sent_frames += 1
        start_sample += SAMPLES_PER_FRAME
        drain_status(transport)
        report_started = report_progress(sent_frames, report_started)

        deadline += FRAME_MS / 1000.0
        remaining = deadline - time.monotonic()
        if remaining > 0:
            time.sleep(remaining)
        else:
            deadline = time.monotonic()


def stream_loopback(
    transport: serial.Serial,
    stream_id: int,
    device_name: str | None,
    volume: float,
) -> None:
    capture = select_loopback(device_name)
    print(f"Capture device: {capture.name}")
    sequence = 0
    sent_frames = 0
    report_started = time.monotonic()
    decimator = StreamingDecimator()

    with capture.recorder(
        samplerate=SOURCE_RATE,
        channels=1,
        blocksize=SOURCE_FRAMES * 4,
    ) as recorder:
        while True:
            source = recorder.record(numframes=SOURCE_FRAMES)
            pcm = decimator.convert(source, volume)
            send_pcm_frame(transport, stream_id, sequence, pcm)
            sequence = (sequence + 1) & 0xFFFF_FFFF
            sent_frames += 1
            drain_status(transport)
            report_started = report_progress(sent_frames, report_started)


def run_sender(config: SenderConfig) -> None:
    print(f"Serial port: {config.port} @ {SERIAL_BAUD}")
    print(f"Stream ID: 0x{config.stream_id:08X}")
    if config.tone_hz is not None:
        print(f"Source: {config.tone_hz:g} Hz test tone")
    else:
        print("Source: Windows playback loopback")
    print("Press Ctrl+C to stop.")

    with serial.Serial(
        config.port,
        SERIAL_BAUD,
        timeout=0.05,
        write_timeout=1.0,
    ) as transport:
        time.sleep(1.5)
        transport.reset_input_buffer()
        transport.reset_output_buffer()

        if config.tone_hz is not None:
            stream_test_tone(
                transport,
                config.stream_id,
                config.tone_hz,
                config.volume,
            )
        else:
            stream_loopback(
                transport,
                config.stream_id,
                config.device_name,
                config.volume,
            )


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Stream audio to the PR1 SX1280 transmitter."
    )
    parser.add_argument("--port", help="Transmitter port, for example COM7")
    parser.add_argument(
        "--stream-id",
        type=lambda value: int(value, 0),
        help="Optional uint32 stream ID (decimal or 0x-prefixed).",
    )
    parser.add_argument(
        "--volume",
        type=float,
        default=0.10,
        help="Input scaling from 0.0 to 1.0; start at the 0.10 default.",
    )
    parser.add_argument(
        "--device",
        help="Substring of a Windows loopback/capture device name.",
    )
    parser.add_argument(
        "--tone-hz",
        type=float,
        help="Generate a deterministic test tone instead of capturing playback.",
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
    if args.tone_hz is not None and args.tone_hz <= 0.0:
        print("--tone-hz must be greater than zero.", file=sys.stderr)
        return 2
    if args.stream_id is not None and not 0 <= args.stream_id <= 0xFFFF_FFFF:
        print("--stream-id must fit in an unsigned 32-bit value.", file=sys.stderr)
        return 2

    try:
        stream_id = (
            args.stream_id
            if args.stream_id is not None
            else secrets.randbits(32)
        )
        run_sender(
            SenderConfig(
                port=args.port,
                stream_id=stream_id,
                volume=args.volume,
                device_name=args.device,
                tone_hz=args.tone_hz,
            )
        )
    except KeyboardInterrupt:
        print("\nStopped.")
        return 0
    except Exception as exc:
        print(f"\nError: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
