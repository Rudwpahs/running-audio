#include <array>
#include <cassert>
#include <cstdint>
#include <iostream>
#include "../firmware/common/pr1_afh.hpp"
#include "../firmware/common/pr1_arq.hpp"
#include "../firmware/common/pr1_jitter.hpp"

int main() {
  pr1::afh::ScheduleConfig cfg{};
  cfg.map_version = 42;
  cfg.map.bits = pr1::afh::kChannelMask & ~(1ULL << 5U) & ~(1ULL << 6U);
  pr1::afh::Scheduler scheduler(cfg);

  pr1::arq::Feedback feedback{};
  feedback.rx_highest_seq = 201;
  feedback.recent_loss_bitmap = 1U;
  feedback.map_version = scheduler.current().map_version;

  pr1::arq::RepairRequest request{};
  request.enabled = true;
  request.sequence = 200;
  request.current_map_version = scheduler.current().map_version;
  request.now_us = 100000U;
  request.playout_deadline_us = 108000U;
  request.feedback_age_us = 800U;
  request.max_feedback_age_us = 4000U;
  request.queue_delay_us = 400U;
  request.hop_settle_us = 100U;
  request.airtime_us = 1200U;
  request.decode_margin_us = 300U;
  request.deadline_guard_us = 500U;
  request.frame_airtime_used_us = 1000U;
  request.frame_airtime_budget_us = 5000U;
  request.active_channel_bits = scheduler.current().map.bits;

  pr1::arq::RetransmissionTracker<> tracker;
  pr1::arq::Stats stats{};
  const auto decision = pr1::arq::evaluateAndReserve(feedback, request, &tracker, &stats);
  assert(decision.retransmit);
  assert(scheduler.current().map.isActive(decision.repair_channel));
  assert(decision.repair_channel != 5U && decision.repair_channel != 6U);

  pr1::jitter::Buffer<4> buffer;
  buffer.setAnchor(200, 108000U);
  const std::array<std::uint8_t, 4> payload{{1, 2, 3, 4}};
  assert(buffer.insert(200, payload.data(), payload.size(), 104000U));
  assert(!buffer.insert(200, payload.data(), payload.size(), 105000U));
  assert(buffer.duplicates() == 1U);
  stats.recordDuplicate();

  pr1::arq::Feedback late_feedback{};
  late_feedback.rx_highest_seq = 202;
  late_feedback.recent_loss_bitmap = 1U;
  late_feedback.map_version = scheduler.current().map_version;
  pr1::arq::RepairRequest late_request = request;
  late_request.sequence = 201;
  late_request.now_us = 110000U;
  late_request.playout_deadline_us = 118000U;
  const auto late_decision = pr1::arq::evaluateAndReserve(late_feedback, late_request, &tracker, &stats);
  assert(late_decision.retransmit);

  assert(!buffer.insert(201, payload.data(), payload.size(), 118000U));
  assert(buffer.staleRejected() == 1U);
  stats.recordArrival(false);

  assert(buffer.size() == 1U);
  assert(stats.sent == 2U);
  assert(stats.duplicates == 1U);
  assert(stats.late == 1U);

  std::cout << "test_arq_integration: PASS\n";
}
