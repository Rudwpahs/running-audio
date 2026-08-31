#include <array>
#include <cassert>
#include <cstdint>
#include <iostream>
#include "../firmware/common/pr1_afh.hpp"
#include "../firmware/common/pr1_arq.hpp"
#include "../firmware/common/pr1_fec.hpp"
#include "../firmware/common/pr1_jitter.hpp"
#include "../firmware/common/pr1_link_controller.hpp"
#include "../firmware/common/pr1_packet.hpp"
int main() {
  pr1::afh::ScheduleConfig cfg{}; cfg.session_seed=123; cfg.session_id=1; cfg.map_version=1; pr1::afh::Scheduler sched(cfg);
  for(std::uint32_t seq=1;seq<200;++seq) assert(sched.channelForSequence(seq)!=sched.channelForSequence(seq-1));
  assert(pr1::kDartPacketBytes<=pr1::kRadioPayloadMaxBytes);
  const auto fast=pr1::phy::estimateAirtimeUs(pr1::phy::ProfileId::Flrc1300Cr34,pr1::kDartPacketBytes);
  pr1::arq::RepairBudget rb{}; rb.remaining_slack_us=10000; rb.airtime_us=fast; rb.decode_guard_us=1000; rb.frame_airtime_budget_us=7000; assert(pr1::arq::shouldRetransmit(rb));
  pr1::controller::LinkController ctl; pr1::controller::Metrics m{}; m.bad_channel_permille=250; m.rssi_margin_db_x10=150; ctl.update(m,1000); assert(ctl.state()==pr1::controller::State::Interference);
  std::cout << "test_integration: PASS\n";
}
