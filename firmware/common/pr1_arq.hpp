#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace pr1::arq {

constexpr std::size_t kFeedbackBytes = 10;
constexpr std::uint32_t kDefaultMaxFeedbackAgeUs = 4000U;
constexpr std::uint8_t kRepairChannelCount = 40U;
constexpr std::uint64_t kRepairChannelMask = (1ULL << kRepairChannelCount) - 1ULL;
constexpr std::uint8_t kNoRepairChannel = 0xFFU;

struct Feedback {
  std::uint16_t rx_highest_seq = 0;
  std::uint32_t recent_loss_bitmap = 0;
  std::int8_t rssi_dbm = 0;
  std::uint16_t map_version = 0;
  std::uint8_t buffer_frames = 0;
};

inline void encodeFeedback(const Feedback& f, std::array<std::uint8_t, kFeedbackBytes>* out) {
  if (out == nullptr) return;
  (*out)[0] = static_cast<std::uint8_t>(f.rx_highest_seq >> 8U);
  (*out)[1] = static_cast<std::uint8_t>(f.rx_highest_seq);
  (*out)[2] = static_cast<std::uint8_t>(f.recent_loss_bitmap >> 24U);
  (*out)[3] = static_cast<std::uint8_t>(f.recent_loss_bitmap >> 16U);
  (*out)[4] = static_cast<std::uint8_t>(f.recent_loss_bitmap >> 8U);
  (*out)[5] = static_cast<std::uint8_t>(f.recent_loss_bitmap);
  (*out)[6] = static_cast<std::uint8_t>(f.rssi_dbm);
  (*out)[7] = static_cast<std::uint8_t>(f.map_version >> 8U);
  (*out)[8] = static_cast<std::uint8_t>(f.map_version);
  (*out)[9] = f.buffer_frames;
}

inline Feedback decodeFeedback(const std::array<std::uint8_t, kFeedbackBytes>& in) {
  Feedback f{};
  f.rx_highest_seq = static_cast<std::uint16_t>((static_cast<std::uint16_t>(in[0]) << 8U) | in[1]);
  f.recent_loss_bitmap = (static_cast<std::uint32_t>(in[2]) << 24U) |
                         (static_cast<std::uint32_t>(in[3]) << 16U) |
                         (static_cast<std::uint32_t>(in[4]) << 8U) |
                         static_cast<std::uint32_t>(in[5]);
  f.rssi_dbm = static_cast<std::int8_t>(in[6]);
  f.map_version = static_cast<std::uint16_t>((static_cast<std::uint16_t>(in[7]) << 8U) | in[8]);
  f.buffer_frames = in[9];
  return f;
}

// Bit 0 reports rx_highest_seq-1, bit 31 reports rx_highest_seq-32.
// rx_highest_seq itself is known received and therefore is never a NACK bit.
inline bool feedbackRequestsSequence(const Feedback& feedback, std::uint16_t sequence) {
  const std::uint16_t distance = static_cast<std::uint16_t>(feedback.rx_highest_seq - sequence);
  if (distance == 0U || distance > 32U) return false;
  return ((feedback.recent_loss_bitmap >> (distance - 1U)) & 1U) != 0U;
}

inline std::uint32_t remainingPlayoutSlackUs(std::uint32_t now_us, std::uint32_t deadline_us) {
  const std::int32_t delta = static_cast<std::int32_t>(deadline_us - now_us);
  return delta > 0 ? static_cast<std::uint32_t>(delta) : 0U;
}

inline bool selectRepairChannel(std::uint64_t active_channel_bits, std::uint16_t sequence,
                                std::uint8_t* out_channel) {
  if (out_channel == nullptr) return false;
  const std::uint64_t active = active_channel_bits & kRepairChannelMask;
  if (active == 0ULL) return false;
  const std::uint8_t start = static_cast<std::uint8_t>(sequence % kRepairChannelCount);
  for (std::uint8_t offset = 0; offset < kRepairChannelCount; ++offset) {
    const std::uint8_t channel = static_cast<std::uint8_t>((start + offset) % kRepairChannelCount);
    if (((active >> channel) & 1ULL) != 0ULL) {
      *out_channel = channel;
      return true;
    }
  }
  return false;
}

struct RepairRequest {
  bool enabled = true;
  std::uint16_t sequence = 0;
  std::uint16_t current_map_version = 0;
  std::uint32_t now_us = 0;
  std::uint32_t playout_deadline_us = 0;
  std::uint32_t feedback_age_us = 0;
  std::uint32_t max_feedback_age_us = kDefaultMaxFeedbackAgeUs;
  std::uint32_t queue_delay_us = 0;
  std::uint32_t hop_settle_us = 0;
  std::uint32_t airtime_us = 0;
  std::uint32_t decode_margin_us = 0;
  std::uint32_t deadline_guard_us = 0;
  std::uint32_t frame_airtime_used_us = 0;
  std::uint32_t frame_airtime_budget_us = 0;
  std::uint64_t active_channel_bits = 0;
};

enum class RejectReason : std::uint8_t {
  None,
  Disabled,
  NotNacked,
  StaleFeedback,
  MapVersionMismatch,
  AlreadyRetransmitted,
  Deadline,
  AirtimeBudget,
  NoActiveChannel,
  TrackerUnavailable,
};

struct Decision {
  bool retransmit = false;
  bool nack_requested = false;
  RejectReason reason = RejectReason::None;
  std::uint8_t repair_channel = kNoRepairChannel;
  std::uint32_t remaining_slack_us = 0;
  std::uint32_t estimated_eta_us = 0;
};

inline std::uint32_t futureRepairEtaUs(const RepairRequest& request) {
  const std::uint64_t total = static_cast<std::uint64_t>(request.queue_delay_us) +
                              request.hop_settle_us + request.airtime_us +
                              request.decode_margin_us;
  return total > 0xFFFFFFFFULL ? 0xFFFFFFFFU : static_cast<std::uint32_t>(total);
}

inline Decision evaluateRepair(const Feedback& feedback, const RepairRequest& request,
                               bool already_retransmitted) {
  Decision decision{};
  decision.nack_requested = feedbackRequestsSequence(feedback, request.sequence);
  decision.remaining_slack_us = remainingPlayoutSlackUs(request.now_us, request.playout_deadline_us);
  decision.estimated_eta_us = futureRepairEtaUs(request);

  if (!request.enabled) {
    decision.reason = RejectReason::Disabled;
    return decision;
  }
  if (!decision.nack_requested) {
    decision.reason = RejectReason::NotNacked;
    return decision;
  }
  if (request.max_feedback_age_us == 0U || request.feedback_age_us >= request.max_feedback_age_us) {
    decision.reason = RejectReason::StaleFeedback;
    return decision;
  }
  if (feedback.map_version != request.current_map_version) {
    decision.reason = RejectReason::MapVersionMismatch;
    return decision;
  }
  if (already_retransmitted) {
    decision.reason = RejectReason::AlreadyRetransmitted;
    return decision;
  }

  const std::uint64_t required_us = static_cast<std::uint64_t>(decision.estimated_eta_us) +
                                    request.deadline_guard_us;
  if (decision.remaining_slack_us == 0U ||
      required_us >= static_cast<std::uint64_t>(decision.remaining_slack_us)) {
    decision.reason = RejectReason::Deadline;
    return decision;
  }

  if (request.frame_airtime_budget_us == 0U ||
      request.frame_airtime_used_us > request.frame_airtime_budget_us ||
      request.airtime_us > request.frame_airtime_budget_us - request.frame_airtime_used_us) {
    decision.reason = RejectReason::AirtimeBudget;
    return decision;
  }

  if (!selectRepairChannel(request.active_channel_bits, request.sequence, &decision.repair_channel)) {
    decision.reason = RejectReason::NoActiveChannel;
    return decision;
  }

  decision.retransmit = true;
  decision.reason = RejectReason::None;
  return decision;
}

template <std::size_t Capacity = 64>
class RetransmissionTracker {
 public:
  static_assert(Capacity >= 32, "tracker must cover the entire NACK window");

  bool contains(std::uint16_t sequence) const {
    for (std::size_t i = 0; i < Capacity; ++i) {
      if (valid_[i] && sequences_[i] == sequence) return true;
    }
    return false;
  }

  bool mark(std::uint16_t sequence) {
    if (contains(sequence)) return false;
    sequences_[write_] = sequence;
    valid_[write_] = true;
    write_ = (write_ + 1U) % Capacity;
    return true;
  }

 private:
  std::array<std::uint16_t, Capacity> sequences_{};
  std::array<bool, Capacity> valid_{};
  std::size_t write_ = 0;
};

struct Stats {
  std::uint32_t requested = 0;
  std::uint32_t sent = 0;
  std::uint32_t useful = 0;
  std::uint32_t late = 0;
  std::uint32_t duplicates = 0;
  std::uint32_t rejected_disabled = 0;
  std::uint32_t rejected_not_nacked = 0;
  std::uint32_t rejected_stale_feedback = 0;
  std::uint32_t rejected_map_version = 0;
  std::uint32_t rejected_already_retransmitted = 0;
  std::uint32_t rejected_deadline = 0;
  std::uint32_t rejected_budget = 0;
  std::uint32_t rejected_no_active_channel = 0;
  std::uint32_t rejected_tracker_unavailable = 0;

  void recordDecision(const Decision& decision) {
    if (decision.nack_requested) ++requested;
    if (decision.retransmit) {
      ++sent;
      return;
    }
    switch (decision.reason) {
      case RejectReason::Disabled: ++rejected_disabled; break;
      case RejectReason::NotNacked: ++rejected_not_nacked; break;
      case RejectReason::StaleFeedback: ++rejected_stale_feedback; break;
      case RejectReason::MapVersionMismatch: ++rejected_map_version; break;
      case RejectReason::AlreadyRetransmitted: ++rejected_already_retransmitted; break;
      case RejectReason::Deadline: ++rejected_deadline; break;
      case RejectReason::AirtimeBudget: ++rejected_budget; break;
      case RejectReason::NoActiveChannel: ++rejected_no_active_channel; break;
      case RejectReason::TrackerUnavailable: ++rejected_tracker_unavailable; break;
      case RejectReason::None: break;
    }
  }

  void recordArrival(bool before_deadline) {
    if (before_deadline) ++useful;
    else ++late;
  }

  void recordDuplicate() { ++duplicates; }

  std::uint32_t usefulRatioPpm() const {
    return sent == 0U ? 0U : static_cast<std::uint32_t>((static_cast<std::uint64_t>(useful) * 1000000ULL) / sent);
  }
};

template <std::size_t Capacity>
Decision evaluateAndReserve(const Feedback& feedback, const RepairRequest& request,
                            RetransmissionTracker<Capacity>* tracker, Stats* stats = nullptr) {
  if (tracker == nullptr) {
    Decision unavailable{};
    unavailable.nack_requested = feedbackRequestsSequence(feedback, request.sequence);
    unavailable.remaining_slack_us = remainingPlayoutSlackUs(request.now_us, request.playout_deadline_us);
    unavailable.estimated_eta_us = futureRepairEtaUs(request);
    unavailable.reason = RejectReason::TrackerUnavailable;
    if (stats != nullptr) stats->recordDecision(unavailable);
    return unavailable;
  }

  Decision decision = evaluateRepair(feedback, request, tracker->contains(request.sequence));
  if (decision.retransmit && !tracker->mark(request.sequence)) {
    decision.retransmit = false;
    decision.repair_channel = kNoRepairChannel;
    decision.reason = RejectReason::AlreadyRetransmitted;
  }
  if (stats != nullptr) stats->recordDecision(decision);
  return decision;
}

// Legacy low-level budget helper retained for existing call sites. New live ARQ code
// should use evaluateAndReserve(), which applies NACK, freshness, map and one-shot gates.
struct RepairBudget {
  std::uint32_t remaining_slack_us = 0;
  std::uint32_t feedback_age_us = 0;
  std::uint32_t queue_delay_us = 0;
  std::uint32_t hop_settle_us = 0;
  std::uint32_t airtime_us = 0;
  std::uint32_t decode_guard_us = 0;
  std::uint32_t frame_airtime_used_us = 0;
  std::uint32_t frame_airtime_budget_us = 0;
  bool already_retransmitted = false;
};

inline std::uint32_t estimatedRepairEtaUs(const RepairBudget& b) {
  return b.feedback_age_us + b.queue_delay_us + b.hop_settle_us + b.airtime_us + b.decode_guard_us;
}

inline bool shouldRetransmit(const RepairBudget& b) {
  if (b.already_retransmitted || b.frame_airtime_budget_us == 0U) return false;
  if (b.frame_airtime_used_us > b.frame_airtime_budget_us ||
      b.airtime_us > b.frame_airtime_budget_us - b.frame_airtime_used_us) return false;
  return estimatedRepairEtaUs(b) < b.remaining_slack_us;
}

}  // namespace pr1::arq
