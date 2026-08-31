#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace pr1::arq {

constexpr std::size_t kFeedbackBytes = 10;

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
  if (b.frame_airtime_used_us + b.airtime_us > b.frame_airtime_budget_us) return false;
  return estimatedRepairEtaUs(b) < b.remaining_slack_us;
}

struct Stats {
  std::uint32_t requested = 0;
  std::uint32_t sent = 0;
  std::uint32_t useful = 0;
  std::uint32_t late = 0;
  std::uint32_t rejected_deadline = 0;
  std::uint32_t rejected_budget = 0;

  std::uint32_t usefulRatioPpm() const {
    return sent == 0U ? 0U : static_cast<std::uint32_t>((static_cast<std::uint64_t>(useful) * 1000000ULL) / sent);
  }
};

}  // namespace pr1::arq
