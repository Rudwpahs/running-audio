#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "pr1_afh.hpp"
#include "pr1_packet.hpp"

namespace pr1::fec {

constexpr std::size_t kCodecPayloadBytes = kDartTargetOpusPayloadBytes;
constexpr std::uint8_t kDefaultSourceCount = 4;
constexpr std::uint8_t kSevereSourceCount = 3;
constexpr std::size_t kMetadataBytes = 4;
constexpr std::size_t kParityPayloadBytes = kMetadataBytes + kCodecPayloadBytes;
static_assert(kParityPayloadBytes == 104, "FEC parity payload size changed");
static_assert(kHeaderBytes + kParityPayloadBytes <= kRadioPayloadMaxBytes,
              "FEC parity packet exceeds FLRC payload ceiling");

// Runtime policy: GOOD can keep FEC Off; degraded states can select 4+1 or 3+1.
enum class Mode : std::uint8_t { Off, Xor4Plus1, Xor3Plus1 };

struct Policy {
  Mode mode = Mode::Off;
  bool interleave_depth2 = false;  // experiment only; default OFF.

  bool enabled() const { return mode != Mode::Off; }
  std::uint8_t sourceCount() const {
    switch (mode) {
      case Mode::Xor4Plus1: return kDefaultSourceCount;
      case Mode::Xor3Plus1: return kSevereSourceCount;
      case Mode::Off: break;
    }
    return 0;
  }
};

enum class RecoveryStatus : std::uint8_t {
  Recovered,
  NoMissing,
  TooManyMissing,
  InvalidMetadata,
  InvalidArgument,
};

struct Stats {
  std::uint32_t parity_sent = 0;
  std::uint32_t recovered = 0;
  std::uint32_t unrecoverable = 0;
  std::uint32_t rejected_ambiguous = 0;
};

template <std::uint8_t SourceCount>
struct ParityFrame {
  static_assert(SourceCount >= 2 && SourceCount <= 8,
                "unsupported XOR FEC group size");
  std::uint16_t group_id = 0;
  std::uint8_t source_count = SourceCount;
  std::uint8_t source_bitmap = static_cast<std::uint8_t>((1U << SourceCount) - 1U);
  std::array<std::uint8_t, kCodecPayloadBytes> parity{};
};

template <std::uint8_t SourceCount>
constexpr std::uint8_t expectedBitmap() {
  static_assert(SourceCount >= 2 && SourceCount <= 8,
                "unsupported XOR FEC group size");
  return static_cast<std::uint8_t>((1U << SourceCount) - 1U);
}

template <std::uint8_t SourceCount>
inline bool encode(std::uint16_t group_id,
                   const std::array<const std::uint8_t*, SourceCount>& sources,
                   ParityFrame<SourceCount>* out) {
  if (out == nullptr) return false;
  for (const auto* source : sources) {
    if (source == nullptr) return false;
  }
  out->group_id = group_id;
  out->source_count = SourceCount;
  out->source_bitmap = expectedBitmap<SourceCount>();
  out->parity.fill(0);
  for (std::uint8_t s = 0; s < SourceCount; ++s) {
    for (std::size_t i = 0; i < kCodecPayloadBytes; ++i) {
      out->parity[i] ^= sources[s][i];
    }
  }
  return true;
}

template <std::uint8_t SourceCount>
inline RecoveryStatus recoverOneDetailed(
    const ParityFrame<SourceCount>& parity,
    const std::array<const std::uint8_t*, SourceCount>& sources,
    std::uint8_t* recovered_index,
    std::array<std::uint8_t, kCodecPayloadBytes>* recovered_payload,
    Stats* stats = nullptr) {
  if (recovered_index == nullptr || recovered_payload == nullptr) {
    if (stats != nullptr) ++stats->rejected_ambiguous;
    return RecoveryStatus::InvalidArgument;
  }
  if (parity.source_count != SourceCount ||
      parity.source_bitmap != expectedBitmap<SourceCount>()) {
    if (stats != nullptr) ++stats->rejected_ambiguous;
    return RecoveryStatus::InvalidMetadata;
  }

  std::uint8_t missing = SourceCount;
  std::uint8_t missing_count = 0;
  for (std::uint8_t s = 0; s < SourceCount; ++s) {
    if (sources[s] == nullptr) {
      missing = s;
      ++missing_count;
    }
  }
  if (missing_count == 0U) {
    if (stats != nullptr) ++stats->rejected_ambiguous;
    return RecoveryStatus::NoMissing;
  }
  if (missing_count != 1U) {
    if (stats != nullptr) ++stats->unrecoverable;
    return RecoveryStatus::TooManyMissing;
  }

  *recovered_payload = parity.parity;
  for (std::uint8_t s = 0; s < SourceCount; ++s) {
    if (s == missing) continue;
    for (std::size_t i = 0; i < kCodecPayloadBytes; ++i) {
      (*recovered_payload)[i] ^= sources[s][i];
    }
  }
  *recovered_index = missing;
  if (stats != nullptr) ++stats->recovered;
  return RecoveryStatus::Recovered;
}

// Same recovery operation, but with an explicit expected group ID so a parity
// frame from a stale/different FEC group cannot be applied accidentally.
template <std::uint8_t SourceCount>
inline RecoveryStatus recoverOneForGroup(
    std::uint16_t expected_group_id,
    const ParityFrame<SourceCount>& parity,
    const std::array<const std::uint8_t*, SourceCount>& sources,
    std::uint8_t* recovered_index,
    std::array<std::uint8_t, kCodecPayloadBytes>* recovered_payload,
    Stats* stats = nullptr) {
  if (parity.group_id != expected_group_id) {
    if (stats != nullptr) ++stats->rejected_ambiguous;
    return RecoveryStatus::InvalidMetadata;
  }
  return recoverOneDetailed<SourceCount>(parity, sources, recovered_index,
                                         recovered_payload, stats);
}

template <std::uint8_t SourceCount>
inline bool recoverOne(
    const ParityFrame<SourceCount>& parity,
    const std::array<const std::uint8_t*, SourceCount>& sources,
    std::uint8_t* recovered_index,
    std::array<std::uint8_t, kCodecPayloadBytes>* recovered_payload) {
  return recoverOneDetailed<SourceCount>(parity, sources, recovered_index, recovered_payload) ==
         RecoveryStatus::Recovered;
}

// Encode the FEC metadata + 100-byte codec parity as one application payload.
// The normal PR1 outer packet header can wrap this 104-byte payload and still
// remain below the 127-byte FLRC payload ceiling.
template <std::uint8_t SourceCount>
inline bool encodeParityPayload(
    const ParityFrame<SourceCount>& parity,
    std::array<std::uint8_t, kParityPayloadBytes>* out) {
  if (out == nullptr || parity.source_count != SourceCount ||
      parity.source_bitmap != expectedBitmap<SourceCount>()) {
    return false;
  }
  (*out)[0] = static_cast<std::uint8_t>(parity.group_id >> 8U);
  (*out)[1] = static_cast<std::uint8_t>(parity.group_id & 0xFFU);
  (*out)[2] = parity.source_count;
  (*out)[3] = parity.source_bitmap;
  for (std::size_t i = 0; i < kCodecPayloadBytes; ++i) {
    (*out)[kMetadataBytes + i] = parity.parity[i];
  }
  return true;
}

template <std::uint8_t SourceCount>
inline bool decodeParityPayload(const std::uint8_t* data, std::size_t len,
                                ParityFrame<SourceCount>* out) {
  if (data == nullptr || out == nullptr || len != kParityPayloadBytes ||
      data[2] != SourceCount || data[3] != expectedBitmap<SourceCount>()) {
    return false;
  }
  out->group_id = static_cast<std::uint16_t>(
      (static_cast<std::uint16_t>(data[0]) << 8U) | data[1]);
  out->source_count = data[2];
  out->source_bitmap = data[3];
  for (std::size_t i = 0; i < kCodecPayloadBytes; ++i) {
    out->parity[i] = data[kMetadataBytes + i];
  }
  return true;
}

// Streaming encoder: parity becomes available on the exact call that accepts
// the Nth source frame. Firmware can therefore enqueue the repair subslot
// immediately after the 4th source (or 3rd in severe-link experiment mode).
template <std::uint8_t SourceCount>
class StreamingEncoder {
 public:
  explicit StreamingEncoder(std::uint16_t first_group_id = 0,
                            Stats* stats = nullptr)
      : group_id_(first_group_id), stats_(stats) {
    accumulator_.fill(0);
  }

  std::uint16_t currentGroupId() const { return group_id_; }
  std::uint8_t sourceIndex() const { return source_index_; }

  void reset(std::uint16_t next_group_id) {
    group_id_ = next_group_id;
    source_index_ = 0;
    accumulator_.fill(0);
  }

  bool push(const std::uint8_t* source, ParityFrame<SourceCount>* completed) {
    if (source == nullptr || completed == nullptr) return false;
    for (std::size_t i = 0; i < kCodecPayloadBytes; ++i) {
      accumulator_[i] ^= source[i];
    }
    ++source_index_;
    if (source_index_ < SourceCount) return false;

    completed->group_id = group_id_;
    completed->source_count = SourceCount;
    completed->source_bitmap = expectedBitmap<SourceCount>();
    completed->parity = accumulator_;
    if (stats_ != nullptr) ++stats_->parity_sent;

    ++group_id_;
    source_index_ = 0;
    accumulator_.fill(0);
    return true;
  }

 private:
  std::uint16_t group_id_ = 0;
  std::uint8_t source_index_ = 0;
  std::array<std::uint8_t, kCodecPayloadBytes> accumulator_{};
  Stats* stats_ = nullptr;
};

// Choose a deterministic ACTIVE channel different from the primary packet
// when possible. Returning false means there is no distinct repair channel.
inline bool chooseRepairChannel(const afh::ChannelMap& map,
                                std::uint8_t primary_channel,
                                std::uint32_t salt,
                                std::uint8_t* out_channel) {
  if (out_channel == nullptr || primary_channel >= afh::kChannelCount) return false;
  if (map.activeCount() < 2U) return false;

  // 17 is coprime with 40, so repeated offsets walk every channel before
  // repeating and tend to separate repair traffic from the primary frequency.
  const std::uint8_t start = static_cast<std::uint8_t>(
      (static_cast<std::uint32_t>(primary_channel) + 17U + (salt % afh::kChannelCount)) %
      afh::kChannelCount);
  for (std::uint8_t step = 0; step < afh::kChannelCount; ++step) {
    const std::uint8_t candidate = static_cast<std::uint8_t>(
        (static_cast<std::uint16_t>(start) + static_cast<std::uint16_t>(step) * 17U) %
        afh::kChannelCount);
    if (candidate != primary_channel && map.isActive(candidate)) {
      *out_channel = candidate;
      return true;
    }
  }
  return false;
}

}  // namespace pr1::fec
