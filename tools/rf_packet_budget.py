"""Calculate PR1 application + RF-transport packet budgets.

This is a design calculator, not an RF-performance or regulatory simulator.
It deliberately excludes PHY overhead, airtime, interference, retries and legal
operating limits; those must be measured/verified separately.

Run either form:
    python tools/rf_packet_budget.py
    python -m tools.rf_packet_budget
"""

from __future__ import annotations

import math
from dataclasses import dataclass

try:
    # Module form: python -m tools.rf_packet_budget
    from tools.pr1_rf_protocol import HEADER_SIZE as RF_TRANSPORT_HEADER_BYTES
except ModuleNotFoundError:
    # Direct-script form: python tools/rf_packet_budget.py
    from pr1_rf_protocol import HEADER_SIZE as RF_TRANSPORT_HEADER_BYTES

FLRC_MAX_PAYLOAD = 127
LORA_GFSK_MAX_PAYLOAD = 255
CURRENT_APP_HEADER_BYTES = 16


@dataclass(frozen=True)
class AudioProfile:
    name: str
    sample_rate_hz: int
    bit_depth: int
    channels: int
    frame_ms: int
    encoded_fraction_of_pcm: float = 1.0
    codec_state_bytes: int = 0
    app_header_bytes: int = CURRENT_APP_HEADER_BYTES

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
    def estimated_encoded_audio_bytes_per_frame(self) -> int:
        return math.ceil(
            self.raw_pcm_bytes_per_frame * self.encoded_fraction_of_pcm
            + self.codec_state_bytes
        )

    @property
    def application_packet_bytes(self) -> int:
        return self.app_header_bytes + self.estimated_encoded_audio_bytes_per_frame


@dataclass(frozen=True)
class PacketBudget:
    usable_transport_payload_bytes: int
    fragments_per_application_packet: int
    rf_packets_per_second: float
    application_bytes_per_second: float
    transport_bytes_per_second: float


def calculate(profile: AudioProfile, max_rf_payload: int) -> PacketBudget:
    usable = max_rf_payload - RF_TRANSPORT_HEADER_BYTES
    if usable <= 0:
        raise ValueError("RF payload is smaller than the RF transport header")

    application_bytes = profile.application_packet_bytes
    fragments = max(1, math.ceil(application_bytes / usable))
    frames_per_second = 1000 / profile.frame_ms
    packets_per_second = frames_per_second * fragments
    application_per_second = application_bytes * frames_per_second
    transport_per_second = (
        application_per_second
        + RF_TRANSPORT_HEADER_BYTES * packets_per_second
    )

    return PacketBudget(
        usable_transport_payload_bytes=usable,
        fragments_per_application_packet=fragments,
        rf_packets_per_second=packets_per_second,
        application_bytes_per_second=application_per_second,
        transport_bytes_per_second=transport_per_second,
    )


def main() -> None:
    profiles = [
        # Canonical current main profile: 16-B application header + 160-B PCM.
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

    print(f"Application header: {CURRENT_APP_HEADER_BYTES} bytes")
    print(f"RF transport header: {RF_TRANSPORT_HEADER_BYTES} bytes")
    print()
    print(
        "Profile | Mode | Raw audio B/frame | Encoded audio B/frame | "
        "Application packet B | RF fragments/app packet | RF packets/s"
    )
    print("-" * 155)

    for profile in profiles:
        for mode, max_payload in modes:
            result = calculate(profile, max_payload)
            print(
                f"{profile.name} | {mode} | "
                f"{profile.raw_pcm_bytes_per_frame} | "
                f"{profile.estimated_encoded_audio_bytes_per_frame} | "
                f"{profile.application_packet_bytes} | "
                f"{result.fragments_per_application_packet} | "
                f"{result.rf_packets_per_second:.0f}"
            )


if __name__ == "__main__":
    main()
