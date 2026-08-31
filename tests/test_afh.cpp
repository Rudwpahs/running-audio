#include <array>
#include <cassert>
#include <cstdint>
#include <iostream>
#include "../firmware/common/pr1_afh.hpp"
int main() {
  pr1::afh::ScheduleConfig cfg{}; cfg.session_seed = 0x123456789ULL; cfg.session_id = 42; cfg.map_version = 1;
  pr1::afh::Scheduler a(cfg), b(cfg);
  std::array<std::uint32_t, pr1::afh::kChannelCount> counts{};
  std::uint8_t prev = 255;
  for (std::uint32_t seq = 0; seq < 40000; ++seq) {
    const auto ca = a.channelForSequence(seq), cb = b.channelForSequence(seq);
    assert(ca == cb && ca < pr1::afh::kChannelCount);
    if (seq) assert(ca != prev);
    prev = ca; ++counts[ca];
  }
  for (auto c : counts) assert(c == 1000);
  pr1::afh::ChannelMap m{}; m.bits &= ~(1ULL << 5); m.bits &= ~(1ULL << 6);
  assert(a.stageMap(2, m, 100));
  a.applyPendingIfDue(99); assert(a.current().map_version == 1);
  a.applyPendingIfDue(100); assert(a.current().map_version == 2);
  assert(!a.stageMap(2, m, 200));
  assert(a.rendezvousChannel(0) != a.rendezvousChannel(1));
  assert(a.beaconMatchesSession({42,1,0,2}));
  std::cout << "test_afh: PASS\n";
}
