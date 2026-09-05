#include <array>
#include <cassert>
#include <cstdint>
#include <iostream>
#include "../firmware/common/pr1_arq.hpp"

int main() {
  using namespace pr1::arq;

  Feedback f{};
  f.rx_highest_seq = 65535;
  f.recent_loss_bitmap = 0x80000001U;
  f.rssi_dbm = -77;
  f.map_version = 22;
  f.buffer_frames = 4;
  std::array<std::uint8_t, kFeedbackBytes> wire{};
  encodeFeedback(f, &wire);
  const auto decoded = decodeFeedback(wire);
  assert(decoded.rx_highest_seq == f.rx_highest_seq);
  assert(decoded.recent_loss_bitmap == f.recent_loss_bitmap);
  assert(decoded.rssi_dbm == f.rssi_dbm);
  assert(decoded.map_version == f.map_version);
  assert(decoded.buffer_frames == f.buffer_frames);

  Feedback nack{};
  nack.rx_highest_seq = 100;
  nack.recent_loss_bitmap = (1U << 0U) | (1U << 31U);
  assert(feedbackRequestsSequence(nack, 99));
  assert(feedbackRequestsSequence(nack, 68));
  assert(!feedbackRequestsSequence(nack, 100));
  assert(!feedbackRequestsSequence(nack, 67));

  Feedback wrap{};
  wrap.rx_highest_seq = 1;
  wrap.recent_loss_bitmap = (1U << 1U);
  assert(feedbackRequestsSequence(wrap, 65535));

  assert(remainingPlayoutSlackUs(1000U, 9000U) == 8000U);
  assert(remainingPlayoutSlackUs(9000U, 9000U) == 0U);
  assert(remainingPlayoutSlackUs(100U, 50U) == 0U);
  assert(remainingPlayoutSlackUs(0xFFFFFF00U, 0x00000100U) == 512U);

  RepairRequest request{};
  request.enabled = true;
  request.sequence = 99;
  request.current_map_version = 7;
  request.now_us = 10000;
  request.playout_deadline_us = 18000;
  request.feedback_age_us = 1000;
  request.max_feedback_age_us = 4000;
  request.queue_delay_us = 500;
  request.hop_settle_us = 100;
  request.airtime_us = 1200;
  request.decode_margin_us = 500;
  request.deadline_guard_us = 500;
  request.frame_airtime_used_us = 1000;
  request.frame_airtime_budget_us = 5000;
  request.active_channel_bits = (1ULL << 3U) | (1ULL << 9U);

  Feedback good_fb{};
  good_fb.rx_highest_seq = 100;
  good_fb.recent_loss_bitmap = 1U;
  good_fb.map_version = 7;

  RetransmissionTracker<> tracker;
  Stats stats{};
  const auto allowed = evaluateAndReserve(good_fb, request, &tracker, &stats);
  assert(allowed.retransmit);
  assert(allowed.reason == RejectReason::None);
  assert(allowed.repair_channel == 3U || allowed.repair_channel == 9U);
  assert(((request.active_channel_bits >> allowed.repair_channel) & 1ULL) != 0ULL);
  assert(allowed.remaining_slack_us == 8000U);
  assert(allowed.estimated_eta_us == 2300U);
  assert(stats.requested == 1U && stats.sent == 1U);

  const auto second = evaluateAndReserve(good_fb, request, &tracker, &stats);
  assert(!second.retransmit);
  assert(second.reason == RejectReason::AlreadyRetransmitted);
  assert(stats.rejected_already_retransmitted == 1U);

  RepairRequest stale = request;
  stale.sequence = 98;
  stale.feedback_age_us = stale.max_feedback_age_us;
  Feedback stale_fb = good_fb;
  stale_fb.recent_loss_bitmap = 1U << 1U;
  const auto stale_decision = evaluateRepair(stale_fb, stale, false);
  assert(!stale_decision.retransmit);
  assert(stale_decision.reason == RejectReason::StaleFeedback);

  RepairRequest wrong_map = request;
  wrong_map.sequence = 98;
  const auto map_decision = evaluateRepair(stale_fb, wrong_map, false);
  assert(map_decision.retransmit);
  Feedback wrong_map_fb = stale_fb;
  wrong_map_fb.map_version = 8;
  const auto wrong_map_decision = evaluateRepair(wrong_map_fb, wrong_map, false);
  assert(!wrong_map_decision.retransmit);
  assert(wrong_map_decision.reason == RejectReason::MapVersionMismatch);

  RepairRequest too_late = request;
  too_late.sequence = 98;
  too_late.playout_deadline_us = 12800;
  const auto deadline_decision = evaluateRepair(stale_fb, too_late, false);
  assert(!deadline_decision.retransmit);
  assert(deadline_decision.reason == RejectReason::Deadline);

  RepairRequest no_budget = request;
  no_budget.sequence = 98;
  no_budget.frame_airtime_used_us = 4500;
  const auto budget_decision = evaluateRepair(stale_fb, no_budget, false);
  assert(!budget_decision.retransmit);
  assert(budget_decision.reason == RejectReason::AirtimeBudget);

  RepairRequest no_channel = request;
  no_channel.sequence = 98;
  no_channel.active_channel_bits = 0;
  const auto channel_decision = evaluateRepair(stale_fb, no_channel, false);
  assert(!channel_decision.retransmit);
  assert(channel_decision.reason == RejectReason::NoActiveChannel);

  RepairRequest disabled = request;
  disabled.sequence = 98;
  disabled.enabled = false;
  const auto disabled_decision = evaluateRepair(stale_fb, disabled, false);
  assert(!disabled_decision.retransmit);
  assert(disabled_decision.reason == RejectReason::Disabled);

  Feedback not_nacked = good_fb;
  not_nacked.recent_loss_bitmap = 0U;
  const auto not_nacked_decision = evaluateRepair(not_nacked, request, false);
  assert(!not_nacked_decision.retransmit);
  assert(not_nacked_decision.reason == RejectReason::NotNacked);

  stats.recordArrival(true);
  stats.recordArrival(false);
  assert(stats.useful == 1U && stats.late == 1U);
  assert(stats.usefulRatioPpm() == 1000000U);

  RepairBudget legacy{};
  legacy.remaining_slack_us = 8000;
  legacy.queue_delay_us = 500;
  legacy.hop_settle_us = 100;
  legacy.airtime_us = 1200;
  legacy.decode_guard_us = 500;
  legacy.frame_airtime_budget_us = 5000;
  assert(shouldRetransmit(legacy));

  std::cout << "test_arq: PASS\n";
}
