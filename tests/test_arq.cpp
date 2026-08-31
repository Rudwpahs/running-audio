#include <array>
#include <cassert>
#include <iostream>
#include "../firmware/common/pr1_arq.hpp"
int main() {
  pr1::arq::Feedback f{}; f.rx_highest_seq=65535; f.recent_loss_bitmap=0x80000001U; f.rssi_dbm=-77; f.map_version=22; f.buffer_frames=4;
  std::array<std::uint8_t,pr1::arq::kFeedbackBytes> wire{}; pr1::arq::encodeFeedback(f,&wire); auto d=pr1::arq::decodeFeedback(wire);
  assert(d.rx_highest_seq==f.rx_highest_seq && d.recent_loss_bitmap==f.recent_loss_bitmap && d.rssi_dbm==f.rssi_dbm && d.map_version==22);
  pr1::arq::RepairBudget b{}; b.remaining_slack_us=8000; b.queue_delay_us=500; b.hop_settle_us=100; b.airtime_us=1200; b.decode_guard_us=500; b.frame_airtime_budget_us=5000;
  assert(pr1::arq::shouldRetransmit(b)); b.remaining_slack_us=1500; assert(!pr1::arq::shouldRetransmit(b)); b.remaining_slack_us=8000; b.already_retransmitted=true; assert(!pr1::arq::shouldRetransmit(b));
  std::cout << "test_arq: PASS\n";
}
