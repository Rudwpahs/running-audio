#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <iostream>

#include "../firmware/common/pr1_packet.hpp"

namespace {

std::uint32_t xorshift32(std::uint32_t& state) {
  state ^= state << 13U;
  state ^= state >> 17U;
  state ^= state << 5U;
  return state;
}

void test_round_trips() {
  std::array<std::uint8_t, pr1::kV0AudioBytesPerFrame> payload{};
  std::array<std::uint8_t, pr1::kRadioPayloadMaxBytes> wire{};
  std::uint32_t rng = 0x12345678U;

  for (std::uint32_t i = 0; i < 10000U; ++i) {
    for (auto& byte : payload) {
      byte = static_cast<std::uint8_t>(xorshift32(rng) & 0xFFU);
    }

    pr1::Header header{};
    header.flags = static_cast<std::uint8_t>(i & 0x03U);
    header.stream_id = 1;
    header.sequence = static_cast<std::uint16_t>(i & 0xFFFFU);
    header.sample_rate = pr1::kV0SampleRateHz;
    header.capture_ms = i * pr1::kV0FrameMs;

    const std::size_t encoded = pr1::encode_packet(
        header, payload.data(), payload.size(), wire.data(), wire.size());
    assert(encoded == pr1::kV0PacketBytes);

    pr1::DecodedPacket decoded{};
    assert(pr1::decode_packet(wire.data(), encoded, &decoded));
    assert(decoded.header.flags == header.flags);
    assert(decoded.header.stream_id == header.stream_id);
    assert(decoded.header.sequence == header.sequence);
    assert(decoded.header.sample_rate == header.sample_rate);
    assert(decoded.header.payload_len == payload.size());
    assert(decoded.header.capture_ms == header.capture_ms);

    for (std::size_t j = 0; j < payload.size(); ++j) {
      assert(decoded.payload[j] == payload[j]);
    }
  }
}

void test_rejects_invalid_packets() {
  std::array<std::uint8_t, pr1::kRadioPayloadMaxBytes + 1> bytes{};
  pr1::DecodedPacket decoded{};

  assert(!pr1::decode_packet(nullptr, 0, &decoded));
  assert(!pr1::decode_packet(bytes.data(), pr1::kHeaderBytes - 1, &decoded));
  assert(!pr1::decode_packet(bytes.data(), bytes.size(), &decoded));

  std::array<std::uint8_t, pr1::kV0AudioBytesPerFrame> payload{};
  std::array<std::uint8_t, pr1::kRadioPayloadMaxBytes> wire{};
  pr1::Header header{};
  const std::size_t encoded = pr1::encode_packet(
      header, payload.data(), payload.size(), wire.data(), wire.size());
  assert(encoded == pr1::kV0PacketBytes);

  // Corrupt magic.
  auto bad_magic = wire;
  bad_magic[0] ^= 0xFFU;
  assert(!pr1::decode_packet(bad_magic.data(), encoded, &decoded));

  // Corrupt version.
  auto bad_version = wire;
  bad_version[2] = static_cast<std::uint8_t>(pr1::kVersion + 1U);
  assert(!pr1::decode_packet(bad_version.data(), encoded, &decoded));

  // Lie about payload length.
  auto bad_length = wire;
  bad_length[10] = 0;
  bad_length[11] = 1;
  assert(!pr1::decode_packet(bad_length.data(), encoded, &decoded));

  // Reject payloads too large for the design ceiling.
  std::array<std::uint8_t, pr1::kMaxAudioPayloadBytes + 1> too_large{};
  assert(pr1::encode_packet(header, too_large.data(), too_large.size(),
                            wire.data(), wire.size()) == 0);
}

}  // namespace

int main() {
  test_round_trips();
  test_rejects_invalid_packets();

  std::cout << "PR1 C++ packet codec: PASS\n"
            << "  header bytes: " << pr1::kHeaderBytes << '\n'
            << "  audio bytes/frame: " << pr1::kV0AudioBytesPerFrame << '\n'
            << "  packet bytes: " << pr1::kV0PacketBytes << '\n'
            << "  payload headroom: "
            << (pr1::kRadioPayloadMaxBytes - pr1::kV0PacketBytes) << '\n'
            << "  round trips: 10000\n";
  return 0;
}
