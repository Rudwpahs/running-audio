#pragma once

#include <cstddef>
#include <cstdint>

namespace pr1 {

constexpr std::uint16_t kMagic = 0x5052;
constexpr std::uint8_t kVersion = 1;
constexpr std::size_t kHeaderBytes = 16;
// SX1280 FLRC payload ceiling. Keeping this constant accurate is a hard gate:
// anything larger must be rejected before it reaches the radio driver.
constexpr std::size_t kRadioPayloadMaxBytes = 127;
constexpr std::size_t kMaxAudioPayloadBytes = kRadioPayloadMaxBytes - kHeaderBytes;

constexpr std::uint16_t kDartSampleRateHz = 48000;
constexpr std::uint16_t kDartFrameMs = 10;
constexpr std::size_t kDartTargetOpusPayloadBytes = 100;
constexpr std::size_t kDartPacketBytes = kHeaderBytes + kDartTargetOpusPayloadBytes;

static_assert(kHeaderBytes == 16, "PR1 header size changed unexpectedly");
static_assert(kDartPacketBytes == 116, "PR1-DART baseline packet must be 116 bytes");
static_assert(kDartPacketBytes <= kRadioPayloadMaxBytes,
              "PR1-DART application packet exceeds FLRC payload ceiling");

// Legacy pre-study PCM geometry is retained only as documentation. It must not
// be emitted as a single FLRC packet.
constexpr std::uint16_t kLegacySampleRateHz = 8000;
constexpr std::uint16_t kLegacyFrameMs = 20;
constexpr std::size_t kLegacyAudioBytesPerFrame = 160;
constexpr std::size_t kLegacyPacketBytes = kHeaderBytes + kLegacyAudioBytesPerFrame;
static_assert(kLegacyPacketBytes > kRadioPayloadMaxBytes,
              "Legacy packet unexpectedly fits; revisit migration assumptions");

struct Header {
  std::uint8_t flags = 0;
  std::uint16_t stream_id = 0;
  std::uint16_t sequence = 0;
  std::uint16_t sample_rate = kDartSampleRateHz;
  std::uint16_t payload_len = 0;
  std::uint32_t capture_ms = 0;
};

struct DecodedPacket {
  Header header{};
  const std::uint8_t* payload = nullptr;
};

namespace detail {
inline void write_be16(std::uint8_t* out, std::uint16_t value) {
  out[0] = static_cast<std::uint8_t>((value >> 8U) & 0xFFU);
  out[1] = static_cast<std::uint8_t>(value & 0xFFU);
}
inline void write_be32(std::uint8_t* out, std::uint32_t value) {
  out[0] = static_cast<std::uint8_t>((value >> 24U) & 0xFFU);
  out[1] = static_cast<std::uint8_t>((value >> 16U) & 0xFFU);
  out[2] = static_cast<std::uint8_t>((value >> 8U) & 0xFFU);
  out[3] = static_cast<std::uint8_t>(value & 0xFFU);
}
inline std::uint16_t read_be16(const std::uint8_t* in) {
  return static_cast<std::uint16_t>((static_cast<std::uint16_t>(in[0]) << 8U) |
                                    static_cast<std::uint16_t>(in[1]));
}
inline std::uint32_t read_be32(const std::uint8_t* in) {
  return (static_cast<std::uint32_t>(in[0]) << 24U) |
         (static_cast<std::uint32_t>(in[1]) << 16U) |
         (static_cast<std::uint32_t>(in[2]) << 8U) |
         static_cast<std::uint32_t>(in[3]);
}
}  // namespace detail

inline std::size_t encode_packet(const Header& header,
                                 const std::uint8_t* payload,
                                 std::size_t payload_len,
                                 std::uint8_t* out,
                                 std::size_t out_capacity) {
  if (out == nullptr || payload_len > kMaxAudioPayloadBytes ||
      (payload_len > 0 && payload == nullptr)) {
    return 0;
  }
  const std::size_t total_len = kHeaderBytes + payload_len;
  if (out_capacity < total_len) return 0;

  detail::write_be16(out + 0, kMagic);
  out[2] = kVersion;
  out[3] = header.flags;
  detail::write_be16(out + 4, header.stream_id);
  detail::write_be16(out + 6, header.sequence);
  detail::write_be16(out + 8, header.sample_rate);
  detail::write_be16(out + 10, static_cast<std::uint16_t>(payload_len));
  detail::write_be32(out + 12, header.capture_ms);
  for (std::size_t i = 0; i < payload_len; ++i) out[kHeaderBytes + i] = payload[i];
  return total_len;
}

inline bool decode_packet(const std::uint8_t* data,
                          std::size_t data_len,
                          DecodedPacket* decoded) {
  if (data == nullptr || decoded == nullptr || data_len < kHeaderBytes ||
      data_len > kRadioPayloadMaxBytes) {
    return false;
  }
  if (detail::read_be16(data + 0) != kMagic || data[2] != kVersion) return false;

  Header header{};
  header.flags = data[3];
  header.stream_id = detail::read_be16(data + 4);
  header.sequence = detail::read_be16(data + 6);
  header.sample_rate = detail::read_be16(data + 8);
  header.payload_len = detail::read_be16(data + 10);
  header.capture_ms = detail::read_be32(data + 12);
  if (header.payload_len != data_len - kHeaderBytes ||
      header.payload_len > kMaxAudioPayloadBytes) {
    return false;
  }
  decoded->header = header;
  decoded->payload = data + kHeaderBytes;
  return true;
}

}  // namespace pr1
