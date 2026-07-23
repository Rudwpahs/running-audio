"""PR1 RF application protocol helpers.

This module is intentionally hardware-independent. It lets us validate the
packet format, fragmentation, and reassembly on a PC before touching the
SX1280 radio driver.

Protocol v2 header (10 bytes, network byte order):
  0..1  magic          b'P1'
  2     version        2
  3     flags          codec / future state bits
  4..5  packet_seq     uint16, increments per RF packet
  6..7  frame_seq      uint16, increments per audio/application frame
  8     fragment_index uint8, zero based
  9     fragment_count uint8, 1..255

Payload length is inferred from the received RF packet length. This keeps the
header small enough for constrained packet modes.
"""

from __future__ import annotations

import math
import struct
from dataclasses import dataclass, field
from typing import Optional

MAGIC = b"P1"
VERSION = 2
HEADER_FORMAT = "!2sBBHHBB"
HEADER_SIZE = struct.calcsize(HEADER_FORMAT)


@dataclass(frozen=True)
class PacketHeader:
    flags: int
    packet_seq: int
    frame_seq: int
    fragment_index: int
    fragment_count: int


def _check_uint(name: str, value: int, maximum: int) -> None:
    if not 0 <= value <= maximum:
        raise ValueError(f"{name} out of range: {value}")


def pack_header(
    *,
    flags: int,
    packet_seq: int,
    frame_seq: int,
    fragment_index: int,
    fragment_count: int,
) -> bytes:
    """Serialize one v2 PR1 RF header."""
    _check_uint("flags", flags, 0xFF)
    _check_uint("packet_seq", packet_seq, 0xFFFF)
    _check_uint("frame_seq", frame_seq, 0xFFFF)

    if not 1 <= fragment_count <= 0xFF:
        raise ValueError(f"fragment_count out of range: {fragment_count}")
    if not 0 <= fragment_index < fragment_count:
        raise ValueError(
            f"invalid fragment index/count: {fragment_index}/{fragment_count}"
        )

    return struct.pack(
        HEADER_FORMAT,
        MAGIC,
        VERSION,
        flags,
        packet_seq,
        frame_seq,
        fragment_index,
        fragment_count,
    )


def unpack_header(packet: bytes) -> PacketHeader:
    """Validate and deserialize the header from one received RF packet."""
    if len(packet) < HEADER_SIZE:
        raise ValueError(
            f"packet too short: {len(packet)} bytes, need at least {HEADER_SIZE}"
        )

    magic, version, flags, packet_seq, frame_seq, frag_index, frag_count = (
        struct.unpack(HEADER_FORMAT, packet[:HEADER_SIZE])
    )

    if magic != MAGIC:
        raise ValueError(f"bad magic: {magic!r}")
    if version != VERSION:
        raise ValueError(f"unsupported protocol version: {version}")
    if frag_count == 0 or frag_index >= frag_count:
        raise ValueError(
            f"invalid fragment index/count: {frag_index}/{frag_count}"
        )

    return PacketHeader(
        flags=flags,
        packet_seq=packet_seq,
        frame_seq=frame_seq,
        fragment_index=frag_index,
        fragment_count=frag_count,
    )


def fragment_frame(
    frame: bytes,
    *,
    max_rf_payload: int,
    frame_seq: int,
    packet_seq_start: int = 0,
    flags: int = 0,
) -> list[bytes]:
    """Split one application/audio frame into RF packets.

    max_rf_payload includes the 10-byte PR1 header. It must be set to the
    actual payload limit for the selected SX1280 packet mode.
    """
    usable = max_rf_payload - HEADER_SIZE
    if usable <= 0:
        raise ValueError(
            f"max_rf_payload={max_rf_payload} leaves no application payload"
        )

    fragment_count = max(1, math.ceil(len(frame) / usable))
    if fragment_count > 0xFF:
        raise ValueError(f"frame needs too many fragments: {fragment_count}")

    packets: list[bytes] = []
    for index in range(fragment_count):
        start = index * usable
        chunk = frame[start : start + usable]
        header = pack_header(
            flags=flags,
            packet_seq=(packet_seq_start + index) & 0xFFFF,
            frame_seq=frame_seq & 0xFFFF,
            fragment_index=index,
            fragment_count=fragment_count,
        )
        packets.append(header + chunk)

    return packets


@dataclass
class _FrameAssembly:
    fragment_count: int
    fragments: dict[int, bytes] = field(default_factory=dict)


class Reassembler:
    """Small bounded frame reassembler for PC tests and later firmware parity.

    Incomplete old frames are evicted when max_inflight is reached. Recently
    completed frame sequence numbers are also retained for a bounded window so
    that a duplicate whole frame cannot be emitted twice. Production firmware
    should additionally expire incomplete assemblies using a time deadline.
    """

    def __init__(
        self,
        max_inflight: int = 8,
        completed_history: int = 64,
    ) -> None:
        if max_inflight < 1:
            raise ValueError("max_inflight must be at least 1")
        if completed_history < 1:
            raise ValueError("completed_history must be at least 1")

        self.max_inflight = max_inflight
        self.completed_history = completed_history
        self._frames: dict[int, _FrameAssembly] = {}
        self._order: list[int] = []
        self._completed: set[int] = set()
        self._completed_order: list[int] = []

    def _remember_completed(self, frame_seq: int) -> None:
        self._completed.add(frame_seq)
        self._completed_order.append(frame_seq)

        if len(self._completed_order) > self.completed_history:
            oldest = self._completed_order.pop(0)
            self._completed.discard(oldest)

    def push(self, packet: bytes) -> Optional[tuple[int, bytes]]:
        header = unpack_header(packet)
        payload = packet[HEADER_SIZE:]
        frame_seq = header.frame_seq

        if frame_seq in self._completed:
            return None

        if frame_seq not in self._frames:
            if len(self._order) >= self.max_inflight:
                oldest = self._order.pop(0)
                self._frames.pop(oldest, None)
            self._frames[frame_seq] = _FrameAssembly(header.fragment_count)
            self._order.append(frame_seq)

        assembly = self._frames[frame_seq]
        if assembly.fragment_count != header.fragment_count:
            self._frames.pop(frame_seq, None)
            self._order.remove(frame_seq)
            raise ValueError("fragment_count changed within one frame")

        assembly.fragments.setdefault(header.fragment_index, payload)

        if len(assembly.fragments) != assembly.fragment_count:
            return None

        frame = b"".join(
            assembly.fragments[index]
            for index in range(assembly.fragment_count)
        )
        self._frames.pop(frame_seq, None)
        self._order.remove(frame_seq)
        self._remember_completed(frame_seq)
        return frame_seq, frame
