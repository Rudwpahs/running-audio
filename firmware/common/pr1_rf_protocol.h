#pragma once

#include <cstddef>
#include <cstdint>

// Lower-layer RF transport framing.
//
// The canonical PR1 voice/application packet is defined separately in
// `pr1_packet.hpp`. This namespace intentionally avoids colliding with the
// application-layer `pr1::Header` type.
namespace pr1::rf {

constexpr uint8_t kMagic0 = 'P';
constexpr uint8_t kMagic1 = '1';
constexpr uint8_t kTransportVersion = 2;
constexpr std::size_t kTransportHeaderSize = 10;

struct TransportHeader {
  uint8_t flags;
  uint16_t packet_seq;
  uint16_t frame_seq;
  uint8_t fragment_index;
  uint8_t fragment_count;
};

enum class Error : uint8_t {
  kOk = 0,
  kBufferTooSmall,
  kBadMagic,
  kBadVersion,
  kBadFragment,
  kPayloadLimitTooSmall,
  kFragmentIndexOutOfRange,
};

inline void WriteU16Be(uint8_t* p, uint16_t value) {
  p[0] = static_cast<uint8_t>(value >> 8);
  p[1] = static_cast<uint8_t>(value & 0xFF);
}

inline uint16_t ReadU16Be(const uint8_t* p) {
  return static_cast<uint16_t>(
      (static_cast<uint16_t>(p[0]) << 8) | p[1]);
}

inline Error EncodeTransportHeader(const TransportHeader& header, uint8_t* out,
                                   std::size_t out_len) {
  if (out == nullptr || out_len < kTransportHeaderSize) {
    return Error::kBufferTooSmall;
  }
  if (header.fragment_count == 0 ||
      header.fragment_index >= header.fragment_count) {
    return Error::kBadFragment;
  }

  out[0] = kMagic0;
  out[1] = kMagic1;
  out[2] = kTransportVersion;
  out[3] = header.flags;
  WriteU16Be(out + 4, header.packet_seq);
  WriteU16Be(out + 6, header.frame_seq);
  out[8] = header.fragment_index;
  out[9] = header.fragment_count;
  return Error::kOk;
}

inline Error DecodeTransportHeader(const uint8_t* data, std::size_t len,
                                   TransportHeader* out) {
  if (data == nullptr || out == nullptr || len < kTransportHeaderSize) {
    return Error::kBufferTooSmall;
  }
  if (data[0] != kMagic0 || data[1] != kMagic1) {
    return Error::kBadMagic;
  }
  if (data[2] != kTransportVersion) {
    return Error::kBadVersion;
  }
  if (data[9] == 0 || data[8] >= data[9]) {
    return Error::kBadFragment;
  }

  out->flags = data[3];
  out->packet_seq = ReadU16Be(data + 4);
  out->frame_seq = ReadU16Be(data + 6);
  out->fragment_index = data[8];
  out->fragment_count = data[9];
  return Error::kOk;
}

inline std::size_t PayloadCapacity(std::size_t max_rf_payload) {
  return max_rf_payload > kTransportHeaderSize
             ? max_rf_payload - kTransportHeaderSize
             : 0;
}

inline std::size_t FragmentCount(std::size_t frame_len,
                                 std::size_t max_rf_payload) {
  const std::size_t capacity = PayloadCapacity(max_rf_payload);
  if (capacity == 0) {
    return 0;
  }
  return frame_len == 0 ? 1 : (frame_len + capacity - 1) / capacity;
}

inline Error FragmentBounds(std::size_t frame_len,
                            std::size_t max_rf_payload,
                            std::size_t fragment_index,
                            std::size_t* offset,
                            std::size_t* length) {
  if (offset == nullptr || length == nullptr) {
    return Error::kBufferTooSmall;
  }

  const std::size_t capacity = PayloadCapacity(max_rf_payload);
  if (capacity == 0) {
    return Error::kPayloadLimitTooSmall;
  }

  const std::size_t count = FragmentCount(frame_len, max_rf_payload);
  if (fragment_index >= count || count > 255) {
    return Error::kFragmentIndexOutOfRange;
  }

  *offset = fragment_index * capacity;
  const std::size_t remaining =
      frame_len > *offset ? frame_len - *offset : 0;
  *length = remaining < capacity ? remaining : capacity;
  return Error::kOk;
}

}  // namespace pr1::rf
