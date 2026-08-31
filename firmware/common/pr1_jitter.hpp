#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "pr1_packet.hpp"

namespace pr1::jitter {

constexpr std::uint32_t kFrameUs = 10000U;
constexpr std::uint32_t kDefaultTargetUs = 40000U;
constexpr std::size_t kDefaultCapacity = 16;

enum class RecoveryChoice : std::uint8_t { Original, XorFec, Arq, OpusFec, Plc };

struct RecoveryAvailability {
  bool original = false;
  bool xor_fec = false;
  bool arq = false;
  bool opus_fec = false;
};

inline RecoveryChoice chooseRecovery(const RecoveryAvailability& a) {
  if (a.original) return RecoveryChoice::Original;
  if (a.xor_fec) return RecoveryChoice::XorFec;
  if (a.arq) return RecoveryChoice::Arq;
  if (a.opus_fec) return RecoveryChoice::OpusFec;
  return RecoveryChoice::Plc;
}

struct Frame {
  bool valid = false;
  std::uint16_t sequence = 0;
  std::uint32_t arrival_us = 0;
  std::uint32_t deadline_us = 0;
  std::uint16_t payload_len = 0;
  std::array<std::uint8_t, kMaxAudioPayloadBytes> payload{};
};

template <std::size_t Capacity = kDefaultCapacity>
class Buffer {
 public:
  static_assert(Capacity >= 4, "jitter buffer capacity is too small");

  void setAnchor(std::uint16_t anchor_seq, std::uint32_t anchor_playout_us,
                 std::uint32_t target_us = kDefaultTargetUs) {
    anchor_seq_ = anchor_seq;
    anchor_playout_us_ = anchor_playout_us;
    target_us_ = target_us;
    anchored_ = true;
  }

  std::uint32_t deadlineFor(std::uint16_t sequence) const {
    if (!anchored_) return 0;
    const std::int16_t delta = static_cast<std::int16_t>(sequence - anchor_seq_);
    if (delta < 0) return anchor_playout_us_;
    return anchor_playout_us_ + static_cast<std::uint32_t>(delta) * kFrameUs;
  }

  bool insert(std::uint16_t sequence, const std::uint8_t* payload,
              std::size_t payload_len, std::uint32_t arrival_us) {
    if (!anchored_ || payload == nullptr || payload_len > kMaxAudioPayloadBytes) return false;
    const std::uint32_t deadline = deadlineFor(sequence);
    if (static_cast<std::int32_t>(arrival_us - deadline) >= 0) { ++stale_rejected_; return false; }
    for (auto& f : frames_) {
      if (f.valid && f.sequence == sequence) { ++duplicates_; return false; }
    }
    Frame* slot = nullptr;
    for (auto& f : frames_) if (!f.valid) { slot = &f; break; }
    if (slot == nullptr) { ++overflows_; return false; }
    slot->valid = true;
    slot->sequence = sequence;
    slot->arrival_us = arrival_us;
    slot->deadline_us = deadline;
    slot->payload_len = static_cast<std::uint16_t>(payload_len);
    for (std::size_t i = 0; i < payload_len; ++i) slot->payload[i] = payload[i];
    ++size_;
    return true;
  }

  bool take(std::uint16_t sequence, std::uint32_t now_us, Frame* out) {
    if (out == nullptr) return false;
    for (auto& f : frames_) {
      if (f.valid && f.sequence == sequence) {
        if (static_cast<std::int32_t>(now_us - f.deadline_us) > 0) {
          f.valid = false; --size_; ++stale_dropped_; return false;
        }
        *out = f; f.valid = false; --size_; return true;
      }
    }
    return false;
  }

  std::size_t size() const { return size_; }
  std::uint32_t staleRejected() const { return stale_rejected_; }
  std::uint32_t staleDropped() const { return stale_dropped_; }
  std::uint32_t duplicates() const { return duplicates_; }
  std::uint32_t overflows() const { return overflows_; }
  std::uint32_t targetUs() const { return target_us_; }

 private:
  std::array<Frame, Capacity> frames_{};
  bool anchored_ = false;
  std::uint16_t anchor_seq_ = 0;
  std::uint32_t anchor_playout_us_ = 0;
  std::uint32_t target_us_ = kDefaultTargetUs;
  std::size_t size_ = 0;
  std::uint32_t stale_rejected_ = 0;
  std::uint32_t stale_dropped_ = 0;
  std::uint32_t duplicates_ = 0;
  std::uint32_t overflows_ = 0;
};

}  // namespace pr1::jitter
