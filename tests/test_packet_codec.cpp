#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include "../firmware/common/pr1_packet.hpp"

int main() {
  static_assert(pr1::kRadioPayloadMaxBytes == 127);
  static_assert(pr1::kDartPacketBytes == 116);
  std::array<std::uint8_t, pr1::kDartTargetOpusPayloadBytes> payload{};
  for (std::size_t i = 0; i < payload.size(); ++i) payload[i] = static_cast<std::uint8_t>(i);
  std::array<std::uint8_t, pr1::kRadioPayloadMaxBytes> wire{};
  pr1::Header h{}; h.stream_id = 7; h.sequence = 65535; h.capture_ms = 1234;
  const auto n = pr1::encode_packet(h, payload.data(), payload.size(), wire.data(), wire.size());
  assert(n == pr1::kDartPacketBytes);
  pr1::DecodedPacket d{};
  assert(pr1::decode_packet(wire.data(), n, &d));
  assert(d.header.sequence == 65535);
  assert(d.header.sample_rate == 48000);
  assert(d.header.payload_len == payload.size());
  for (std::size_t i = 0; i < payload.size(); ++i) assert(d.payload[i] == payload[i]);

  std::array<std::uint8_t, pr1::kMaxAudioPayloadBytes + 1> too_big{};
  assert(pr1::encode_packet(h, too_big.data(), too_big.size(), wire.data(), wire.size()) == 0);
  assert(!pr1::decode_packet(wire.data(), 128, &d));
  std::cout << "test_packet_codec: PASS\n";
}
