"""Calculate PR1 application-frame packet budgets.

This is a design calculator, not an RF-performance or regulatory simulator.
It deliberately excludes airtime, coding overhead, interference, retries and
legal operating limits; those must be measured/verified separately.

Run:
    python tools/rf_packet_budget.py
"""

from __future__ import annotations

import math
from dataclasses import dataclass

from pr1_rf_protocol import HEADER_SIZE

FLRC_MAX_PAYLOAD = 127
LORA_GFSK_MAX_PAYLOAD = 255


@dataclass(frozen=True)
class AudioProfile:
    name: str
    sample_rate_hz: int
    bit_depth: int
    channels: int
    frame_ms: int
    encoded_fraction_of_pcm: float = 1.0
    codec_state_bytes: int = 0

    @property
    def raw_pcm_bytes_per_frame(self) -> int:
        value = (
            self.sample_rate_hz
            * (self.bit_depth / 8)
            * self.channels
            * (self.frame_ms / 1000)
        )
        return int(round(value))

    @property
    def estimated_encoded_bytes_per_frame(self) -> int:
        return math.ceil(
            self.raw_pcm_bytes_per_frame * self.encoded_fraction_of_pcm
            + self.codec_state_bytes
        )


@dataclass(frozen=True)
class PacketBudget:
    usable_bytes_per_packet: int
    fragments_per_frame: int
    rf_packets_per_second: float
    application_bytes_per_second: float
    pr1_bytes_per_second: float


def calculate(profile: AudioProfile, max_rf_payload: int) -> PacketBudget:
    usable = max_rf_payload - HEADER_SIZE
    if usable <= 0:
        raise ValueError("RF payload is smaller than the PR1 header")

    encoded = profile.estimated_encoded_bytes_per_frame
    fragments = max(1, math.ceil(encoded / usable))
    frames_per_second = 1000 / profile.frame_ms
    packets_per_second = frames_per_second * fragments
    application_per_second = encoded * frames_per_second
    pr1_per_second = application_per_second + HEADER_SIZE * packets_per_second

    return PacketBudget(
        usable_bytes_per_packet=usable,
        fragments_per_frame=fragments,
        rf_packets_per_second=packets_per_second,
        application_bytes_per_second=application_per_second,
        pr1_bytes_per_second=pr1_per_second,
    )


def main() -> None:
    profiles = [
        # Current TECHNICAL_MVP diagnostic profile. Raw first; no codec unless
        # measurement shows a real reason to add one.
        AudioProfile("Voice v0 PCM8 8 kHz mono / 20 ms", 8000, 8, 1, 20),
        AudioProfile("PCM16 16 kHz mono / 10 ms", 16000, 16, 1, 10),
        AudioProfile("PCM16 32 kHz mono / 10 ms", 32000, 16, 1, 10),
        # Fallback experiment only after raw-PCM/modulation measurements.
        AudioProfile(
            "IMA-ADPCM fallback 8 kHz mono / 20 ms",
            8000,
            8,
            1,
            20,
            encoded_fraction_of_pcm=0.5,
            codec_state_bytes=4,
        ),
    ]

    modes = [
        ("FLRC ceiling", FLRC_MAX_PAYLOAD),
        ("LoRa/GFSK ceiling", LORA_GFSK_MAX_PAYLOAD),
    ]

    print(f"PR1 compact protocol header: {HEADER_SIZE} bytes")
    print()
    print(
        "Profile | Mode | Raw B/frame | Estimated app B/frame | "
        "Usable B/RF packet | Fragments/frame | RF packets/s"
    )
    print("-" * 145)

    for profile in profiles:
        for mode, max_payload in modes:
            result = calculate(profile, max_payload)
            print(
                f"{profile.name} | {mode} | "
                f"{profile.raw_pcm_bytes_per_frame} | "
                f"{profile.estimated_encoded_bytes_per_frame} | "
                f"{result.usable_bytes_per_packet} | "
                f"{result.fragments_per_frame} | "
                f"{result.rf_packets_per_second:.0f}"
            )


if __name__ == "__main__":
    main()
