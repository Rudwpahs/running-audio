#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>

#include "pr1_packet.hpp"
#include "pr1_sequence.hpp"

namespace pr1::jitter {

constexpr std::uint64_t kFrameUs = 10000ULL;
constexpr std::uint64_t kDefaultTargetUs = 40000ULL;
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

inline bool sameFrame(const sequence::LogicalFrameId& a,
                      const sequence::LogicalFrameId& b) {
  return a.session_generation == b.session_generation && a.index == b.index;
}

struct Frame {
  bool valid = false;
  sequence::LogicalFrameId id{};
  std::uint64_t arrival_us = 0;
  std::uint64_t deadline_us = 0;
  std::uint16_t payload_len = 0;
  std::array<std::uint8_t, kMaxAudioPayloadBytes> payload{};
};

template <std::size_t Capacity = kDefaultCapacity>
class Buffer {
 public:
  static_assert(Capacity >= 4, "jitter buffer capacity is too small");

  void setAnchor(sequence::LogicalFrameId anchor_frame,
                 std::uint64_t anchor_playout_us,
                 std::uint64_t target_us = kDefaultTargetUs) {
    anchor_frame_ = anchor_frame;
    anchor_playout_us_ = anchor_playout_us;
    target_us_ = target_us;
    anchored_ = true;
    for (auto& frame : frames_) frame.valid = false;
    size_ = 0;
  }

  std::uint64_t deadlineFor(sequence::LogicalFrameId frame) const {
    if (!anchored_ || frame.session_generation != anchor_frame_.session_generation ||
        frame.index < anchor_frame_.index) {
      return 0;
    }

    const std::uint64_t delta = frame.index - anchor_frame_.index;
    constexpr std::uint64_t kMax = std::numeric_limits<std::uint64_t>::max();
    if (delta > (kMax - anchor_playout_us_) / kFrameUs) return kMax;
    return anchor_playout_us_ + delta * kFrameUs;
  }

  bool insert(sequence::LogicalFrameId frame, const std::uint8_t* payload,
              std::size_t payload_len, std::uint64_t arrival_us) {
    if (!anchored_ || payload == nullptr || payload_len > kMaxAudioPayloadBytes ||
        frame.session_generation != anchor_frame_.session_generation ||
        frame.index < anchor_frame_.index) {
      if (anchored_ && (frame.session_generation != anchor_frame_.session_generation ||
                        frame.index < anchor_frame_.index)) {
        ++stale_rejected_;
      }
      return false;
    }

    const std::uint64_t deadline = deadlineFor(frame);
    if (deadline == 0U || arrival_us >= deadline) {
      ++stale_rejected_;
      return false;
    }

    for (auto& stored : frames_) {
      if (stored.valid && sameFrame(stored.id, frame)) {
        ++duplicates_;
        return false;
      }
    }

    Frame* slot = nullptr;
    for (auto& stored : frames_) {
      if (!stored.valid) {
        slot = &stored;
        break;
      }
    }
    if (slot == nullptr) {
      ++overflows_;
      return false;
    }

    slot->valid = true;
    slot->id = frame;
    slot->arrival_us = arrival_us;
    slot->deadline_us = deadline;
    slot->payload_len = static_cast<std::uint16_t>(payload_len);
    for (std::size_t i = 0; i < payload_len; ++i) slot->payload[i] = payload[i];
    ++size_;
    return true;
  }

  bool take(sequence::LogicalFrameId frame, std::uint64_t now_us, Frame* out) {
    if (out == nullptr) return false;
    for (auto& stored : frames_) {
      if (stored.valid && sameFrame(stored.id, frame)) {
        if (now_us > stored.deadline_us) {
          stored.valid = false;
          --size_;
          ++stale_dropped_;
          return false;
        }
        *out = stored;
        stored.valid = false;
        --size_;
        return true;
      }
    }
    return false;
  }

  std::size_t size() const { return size_; }
  std::uint32_t staleRejected() const { return stale_rejected_; }
  std::uint32_t staleDropped() const { return stale_dropped_; }
  std::uint32_t duplicates() const { return duplicates_; }
  std::uint32_t overflows() const { return overflows_; }
  std::uint64_t targetUs() const { return target_us_; }

 private:
  std::array<Frame, Capacity> frames_{};
  bool anchored_ = false;
  sequence::LogicalFrameId anchor_frame_{};
  std::uint64_t anchor_playout_us_ = 0;
  std::uint64_t target_us_ = kDefaultTargetUs;
  std::size_t size_ = 0;
  std::uint32_t stale_rejected_ = 0;
  std::uint32_t stale_dropped_ = 0;
  std::uint32_t duplicates_ = 0;
  std::uint32_t overflows_ = 0;
};

}  // namespace pr1::jitter
