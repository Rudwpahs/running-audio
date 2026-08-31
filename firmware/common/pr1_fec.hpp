#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "pr1_packet.hpp"

namespace pr1::fec {

constexpr std::size_t kCodecPayloadBytes = kDartTargetOpusPayloadBytes;
constexpr std::uint8_t kDefaultSourceCount = 4;
constexpr std::uint8_t kSevereSourceCount = 3;

template <std::uint8_t SourceCount>
struct ParityFrame {
  static_assert(SourceCount >= 2 && SourceCount <= 8, "unsupported XOR FEC group size");
  std::uint16_t group_id = 0;
  std::uint8_t source_count = SourceCount;
  std::uint8_t source_bitmap = static_cast<std::uint8_t>((1U << SourceCount) - 1U);
  std::array<std::uint8_t, kCodecPayloadBytes> parity{};
};

struct Stats {
  std::uint32_t parity_sent = 0;
  std::uint32_t recovered = 0;
  std::uint32_t unrecoverable = 0;
  std::uint32_t rejected_ambiguous = 0;
};

template <std::uint8_t SourceCount>
inline bool encode(std::uint16_t group_id,
                   const std::array<const std::uint8_t*, SourceCount>& sources,
                   ParityFrame<SourceCount>* out) {
  if (out == nullptr) return false;
  for (const auto* source : sources) if (source == nullptr) return false;
  out->group_id = group_id;
  out->source_count = SourceCount;
  out->source_bitmap = static_cast<std::uint8_t>((1U << SourceCount) - 1U);
  out->parity.fill(0);
  for (std::uint8_t s = 0; s < SourceCount; ++s) {
    for (std::size_t i = 0; i < kCodecPayloadBytes; ++i) out->parity[i] ^= sources[s][i];
  }
  return true;
}

template <std::uint8_t SourceCount>
inline bool recoverOne(const ParityFrame<SourceCount>& parity,
                       const std::array<const std::uint8_t*, SourceCount>& sources,
                       std::uint8_t* recovered_index,
                       std::array<std::uint8_t, kCodecPayloadBytes>* recovered_payload) {
  if (recovered_index == nullptr || recovered_payload == nullptr ||
      parity.source_count != SourceCount ||
      parity.source_bitmap != static_cast<std::uint8_t>((1U << SourceCount) - 1U)) {
    return false;
  }
  std::uint8_t missing = SourceCount;
  std::uint8_t missing_count = 0;
  for (std::uint8_t s = 0; s < SourceCount; ++s) {
    if (sources[s] == nullptr) { missing = s; ++missing_count; }
  }
  if (missing_count != 1U) return false;

  *recovered_payload = parity.parity;
  for (std::uint8_t s = 0; s < SourceCount; ++s) {
    if (s == missing) continue;
    for (std::size_t i = 0; i < kCodecPayloadBytes; ++i) (*recovered_payload)[i] ^= sources[s][i];
  }
  *recovered_index = missing;
  return true;
}

}  // namespace pr1::fec
