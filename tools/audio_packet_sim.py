#!/usr/bin/env python3
"""PR1 v0 audio packet codec simulator.

This tool deliberately has no radio dependency. It verifies the application-level
packet shape before the ESP32/SX1280 implementation is written.

v0 profile:
- 8 kHz
- mono
- unsigned PCM 8-bit
- 20 ms frame = 160 audio bytes
- 16-byte application header
- 176-byte application packet

The SX1280 LoRa payload maximum is 255 bytes, so the v0 frame fits without
fragmentation.
"""

from __future__ import annotations

import argparse
import dataclasses
import random
import struct
import sys
from typing import Final

MAGIC: Final[int] = 0x5052  # ASCII-ish "PR"
VERSION: Final[int] = 1
SAMPLE_RATE_HZ: Final[int] = 8_000
FRAME_MS: Final[int] = 20
AUDIO_BYTES_PER_FRAME: Final[int] = SAMPLE_RATE_HZ * FRAME_MS // 1000  # 160
RADIO_PAYLOAD_MAX_BYTES: Final[int] = 255

# magic H, version B, flags B, stream_id H, sequence H,
# sample_rate H, payload_len H, capture_ms I
HEADER_FORMAT: Final[str] = "!HBBHHHHI"
HEADER_BYTES: Final[int] = struct.calcsize(HEADER_FORMAT)
MAX_AUDIO_PAYLOAD_BYTES: Final[int] = RADIO_PAYLOAD_MAX_BYTES - HEADER_BYTES


class PacketError(ValueError):
    """Raised when a packet is invalid or inconsistent."""


@dataclasses.dataclass(frozen=True, slots=True)
class AudioPacket:
    flags: int
    stream_id: int
    sequence: int
    sample_rate: int
    capture_ms: int
    payload: bytes


def encode_packet(packet: AudioPacket) -> bytes:
    """Serialize an AudioPacket into the PR1 v0 wire format."""
    if packet.sample_rate <= 0 or packet.sample_rate > 0xFFFF:
        raise PacketError("sample_rate must fit uint16 and be positive")
    if not 0 <= packet.flags <= 0xFF:
        raise PacketError("flags must fit uint8")
    if not 0 <= packet.stream_id <= 0xFFFF:
        raise PacketError("stream_id must fit uint16")
    if not 0 <= packet.sequence <= 0xFFFF:
        raise PacketError("sequence must fit uint16")
    if not 0 <= packet.capture_ms <= 0xFFFFFFFF:
        raise PacketError("capture_ms must fit uint32")
    if len(packet.payload) > MAX_AUDIO_PAYLOAD_BYTES:
        raise PacketError(
            f"payload too large: {len(packet.payload)} > {MAX_AUDIO_PAYLOAD_BYTES} bytes"
        )

    header = struct.pack(
        HEADER_FORMAT,
        MAGIC,
        VERSION,
        packet.flags,
        packet.stream_id,
        packet.sequence,
        packet.sample_rate,
        len(packet.payload),
        packet.capture_ms,
    )
    encoded = header + packet.payload
    if len(encoded) > RADIO_PAYLOAD_MAX_BYTES:
        raise PacketError("encoded packet exceeds radio payload maximum")
    return encoded


def decode_packet(data: bytes) -> AudioPacket:
    """Parse and validate a PR1 v0 packet."""
    if len(data) < HEADER_BYTES:
        raise PacketError(f"packet too short: {len(data)} < {HEADER_BYTES}")
    if len(data) > RADIO_PAYLOAD_MAX_BYTES:
        raise PacketError(
            f"packet too large: {len(data)} > {RADIO_PAYLOAD_MAX_BYTES} bytes"
        )

    (
        magic,
        version,
        flags,
        stream_id,
        sequence,
        sample_rate,
        payload_len,
        capture_ms,
    ) = struct.unpack(HEADER_FORMAT, data[:HEADER_BYTES])

    if magic != MAGIC:
        raise PacketError(f"bad magic: 0x{magic:04X}")
    if version != VERSION:
        raise PacketError(f"unsupported version: {version}")

    payload = data[HEADER_BYTES:]
    if len(payload) != payload_len:
        raise PacketError(
            f"payload length mismatch: header={payload_len}, actual={len(payload)}"
        )

    return AudioPacket(
        flags=flags,
        stream_id=stream_id,
        sequence=sequence,
        sample_rate=sample_rate,
        capture_ms=capture_ms,
        payload=payload,
    )


def make_test_frame(sequence: int, rng: random.Random) -> AudioPacket:
    """Create a deterministic pseudo-audio frame for round-trip testing."""
    payload = bytes(rng.randrange(0, 256) for _ in range(AUDIO_BYTES_PER_FRAME))
    return AudioPacket(
        flags=0,
        stream_id=1,
        sequence=sequence & 0xFFFF,
        sample_rate=SAMPLE_RATE_HZ,
        capture_ms=(sequence * FRAME_MS) & 0xFFFFFFFF,
        payload=payload,
    )


def run_round_trip_test(packet_count: int, seed: int) -> None:
    rng = random.Random(seed)
    for i in range(packet_count):
        original = make_test_frame(i, rng)
        wire = encode_packet(original)
        decoded = decode_packet(wire)
        if decoded != original:
            raise AssertionError(f"round-trip mismatch at packet {i}")

    packet_bytes = HEADER_BYTES + AUDIO_BYTES_PER_FRAME
    packets_per_second = 1000 // FRAME_MS
    application_bytes_per_second = packet_bytes * packets_per_second
    audio_bits_per_second = AUDIO_BYTES_PER_FRAME * packets_per_second * 8

    print("PR1 audio packet v0: PASS")
    print(f"  header bytes:              {HEADER_BYTES}")
    print(f"  audio bytes/frame:         {AUDIO_BYTES_PER_FRAME}")
    print(f"  total application packet:  {packet_bytes} bytes")
    print(f"  SX1280 payload headroom:   {RADIO_PAYLOAD_MAX_BYTES - packet_bytes} bytes")
    print(f"  packets/second:            {packets_per_second}")
    print(f"  raw audio bitrate:         {audio_bits_per_second / 1000:.1f} kbps")
    print(
        f"  application stream rate:   {application_bytes_per_second * 8 / 1000:.1f} kbps"
    )
    print(f"  round trips verified:      {packet_count}")


def parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--packets",
        type=int,
        default=10_000,
        help="number of encode/decode round trips (default: 10000)",
    )
    parser.add_argument(
        "--seed", type=int, default=1, help="deterministic pseudo-random seed"
    )
    args = parser.parse_args(argv)
    if args.packets <= 0:
        parser.error("--packets must be positive")
    return args


def main(argv: list[str] | None = None) -> int:
    args = parse_args(sys.argv[1:] if argv is None else argv)
    try:
        run_round_trip_test(args.packets, args.seed)
    except (PacketError, AssertionError) as exc:
        print(f"FAIL: {exc}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
