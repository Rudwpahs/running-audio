"""Pure-Python PR1 v3 USB framing shared by the sender and host tests."""

from __future__ import annotations

import struct
import zlib

PROTOCOL_VERSION = 3
AUDIO_FLAG = 0x01
SAMPLE_RATE = 16_000
FRAME_MS = 10
SAMPLES_PER_FRAME = 160
PCM_BYTES_PER_FRAME = SAMPLES_PER_FRAME * 2
SERIAL_MAGIC = b"PR1U"
SERIAL_BAUD = 921_600

_USB_HEADER = struct.Struct("<4sBBHIII")
USB_HEADER_BYTES = _USB_HEADER.size
USB_FRAME_BYTES = USB_HEADER_BYTES + PCM_BYTES_PER_FRAME


def frame_crc32(pcm: bytes) -> int:
    """Return the standard CRC-32 used by the firmware."""
    return zlib.crc32(pcm) & 0xFFFF_FFFF


def build_usb_frame(stream_id: int, sequence: int, pcm: bytes) -> bytes:
    """Encode one fixed-size 10 ms PCM frame for the transmitter."""
    if len(pcm) != PCM_BYTES_PER_FRAME:
        raise ValueError(
            f"PCM must contain {PCM_BYTES_PER_FRAME} bytes, got {len(pcm)}"
        )
    if not 0 <= stream_id <= 0xFFFF_FFFF:
        raise ValueError("stream_id must be an unsigned 32-bit value")
    if not 0 <= sequence <= 0xFFFF_FFFF:
        raise ValueError("sequence must be an unsigned 32-bit value")

    header = _USB_HEADER.pack(
        SERIAL_MAGIC,
        PROTOCOL_VERSION,
        AUDIO_FLAG,
        PCM_BYTES_PER_FRAME,
        stream_id,
        sequence,
        frame_crc32(pcm),
    )
    return header + pcm


def unpack_usb_header(
    frame: bytes,
) -> tuple[bytes, int, int, int, int, int, int]:
    """Expose the header fields for diagnostics and unit tests."""
    if len(frame) < USB_HEADER_BYTES:
        raise ValueError("frame is shorter than the USB header")
    return _USB_HEADER.unpack_from(frame)
