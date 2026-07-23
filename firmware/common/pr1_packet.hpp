#pragma once

#include <cstddef>
#include <cstdint>

namespace pr1 {

constexpr std::uint16_t kMagic = 0x5052;
constexpr std::uint8_t kVersion = 1;
constexpr std::size_t kHeaderBytes = 16;
constexpr std::size_t kRadioPayloadMaxBytes = 255;
constexpr std::size_t kMaxAudioPayloadBytes = kRadioPayloadMaxBytes - kHeaderBytes;

constexpr std::uint16_t kV0SampleRateHz = 8000;
constexpr std::uint16_t kV0FrameMs = 20;
constexpr std::size_t kV0AudioBytesPerFrame =
    (static_cast<std::size_t>(kV0SampleRateHz) * kV0FrameMs) / 1000;
constexpr std::size_t kV0PacketBytes = kHeaderBytes + kV0AudioBytesPerFrame;

static_assert(kHeaderBytes == 16, "PR1 v0 header size changed unexpectedly");
static_assert(kV0AudioBytesPerFrame == 160, "PR1 v0 audio frame must be 160 bytes");
static_assert(kV0PacketBytes == 176, "PR1 v0 packet must be 176 bytes");
static_assert(kV0PacketBytes <= kRadioPayloadMaxBytes,
              "PR1 v0 application packet exceeds the design payload ceiling");

struct Header {
  std::uint8_t flags = 0;
  std::uint16_t stream_id = 0;
  std::uint16_t sequence = 0;
  std::uint16_t sample_rate = kV0SampleRateHz;
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
  return static_cast<std::uint16_t>(
      (static_cast<std::uint16_t>(in[0]) << 8U) |
      static_cast<std::uint16_t>(in[1]));
}

inline std::uint32_t read_be32(const std::uint8_t* in) {
  return (static_cast<std::uint32_t>(in[0]) << 24U) |
         (static_cast<std::uint32_t>(in[1]) << 16U) |
         (static_cast<std::uint32_t>(in[2]) << 8U) |
         static_cast<std::uint32_t>(in[3]);
}

}  // namespace detail

// Encodes a complete application packet into `out`.
// Returns the encoded byte count, or 0 when an argument is invalid.
inline std::size_t encode_packet(const Header& header,
                                 const std::uint8_t* payload,
                                 std::size_t payload_len,
                                 std::uint8_t* out,
                                 std::size_t out_capacity) {
  if (out == nullptr) {
    return 0;
  }
  if (payload_len > kMaxAudioPayloadBytes) {
    return 0;
  }
  if (payload_len > 0 && payload == nullptr) {
    return 0;
  }

  const std::size_t total_len = kHeaderBytes + payload_len;
  if (out_capacity < total_len) {
    return 0;
  }

  detail::write_be16(out + 0, kMagic);
  out[2] = kVersion;
  out[3] = header.flags;
  detail::write_be16(out + 4, header.stream_id);
  detail::write_be16(out + 6, header.sequence);
  detail::write_be16(out + 8, header.sample_rate);
  detail::write_be16(out + 10, static_cast<std::uint16_t>(payload_len));
  detail::write_be32(out + 12, header.capture_ms);

  for (std::size_t i = 0; i < payload_len; ++i) {
    out[kHeaderBytes + i] = payload[i];
  }

  return total_len;
}

// Decodes and validates a complete application packet.
// The payload pointer in `decoded` points into `data`; the caller owns its lifetime.
inline bool decode_packet(const std::uint8_t* data,
                          std::size_t data_len,
                          DecodedPacket* decoded) {
  if (data == nullptr || decoded == nullptr) {
    return false;
  }
  if (data_len < kHeaderBytes || data_len > kRadioPayloadMaxBytes) {
    return false;
  }
  if (detail::read_be16(data + 0) != kMagic || data[2] != kVersion) {
    return false;
  }

  Header header{};
  header.flags = data[3];
  header.stream_id = detail::read_be16(data + 4);
  header.sequence = detail::read_be16(data + 6);
  header.sample_rate = detail::read_be16(data + 8);
  header.payload_len = detail::read_be16(data + 10);
  header.capture_ms = detail::read_be32(data + 12);

  if (header.payload_len != data_len - kHeaderBytes) {
    return false;
  }
  if (header.payload_len > kMaxAudioPayloadBytes) {
    return false;
  }

  decoded->header = header;
  decoded->payload = data + kHeaderBytes;
  return true;
}

}  // namespace pr1
