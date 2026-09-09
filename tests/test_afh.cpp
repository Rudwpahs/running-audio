#include <array>
#include <cassert>
#include <cstdint>
#include <iostream>
#include <type_traits>

#include "../firmware/common/pr1_afh.hpp"
#include "../firmware/common/pr1_sequence.hpp"

int main() {
  static_assert(std::is_same_v<
                decltype(pr1::afh::PendingMap{}.activation_sequence),
                pr1::sequence::LogicalFrameIndex>,
                "AFH activation must use shared logical frame type");

  pr1::afh::ScheduleConfig cfg{};
  cfg.session_seed = 0x123456789ULL;
  cfg.session_id = 42;
  cfg.map_version = 1;
  pr1::afh::Scheduler a(cfg), b(cfg);
  std::array<std::uint32_t, pr1::afh::kChannelCount> counts{};
  std::uint8_t prev = 255;
  for (pr1::sequence::LogicalFrameIndex seq = 0; seq < 40000; ++seq) {
    const auto ca = a.channelForSequence(seq), cb = b.channelForSequence(seq);
    assert(ca == cb && ca < pr1::afh::kChannelCount);
    if (seq) assert(ca != prev);
    prev = ca;
    ++counts[ca];
  }
  for (auto c : counts) assert(c == 1000);

  // Scheduling is deterministic well past the 16-bit audio sequence wrap when
  // both peers share the exact same map/session state.
  pr1::afh::Scheduler stable_a(cfg), stable_b(cfg);
  for (const pr1::sequence::LogicalFrameIndex frame :
       {65535ULL, 65536ULL, 262144ULL, 720000ULL}) {
    assert(stable_a.channelForSequence(frame) ==
           stable_b.channelForSequence(frame));
  }

  pr1::afh::ChannelMap m{};
  m.bits &= ~(1ULL << 5);
  m.bits &= ~(1ULL << 6);
  assert(a.stageMap(2, m, 100));
  a.applyPendingIfDue(99);
  assert(a.current().map_version == 1);
  a.applyPendingIfDue(100);
  assert(a.current().map_version == 2);
  assert(!a.stageMap(2, m, 200));

  // Enforce the shared uint64 timeline itself, not merely the >1 h use case.
  // A uint32 activation field would truncate this future logical frame.
  pr1::afh::ScheduleConfig long_cfg = cfg;
  long_cfg.map_version = 10;
  pr1::afh::Scheduler long_a(long_cfg), long_b(long_cfg);
  pr1::afh::ChannelMap long_map{};
  long_map.bits &= ~(1ULL << 1);
  constexpr pr1::sequence::LogicalFrameIndex kBeyondUint32 =
      (1ULL << 32U) + 42ULL;
  assert(long_a.stageMap(11, long_map, kBeyondUint32));
  assert(long_b.stageMap(11, long_map, kBeyondUint32));
  long_a.applyPendingIfDue(kBeyondUint32 - 1ULL);
  assert(long_a.current().map_version == 10);
  long_a.applyPendingIfDue(kBeyondUint32);
  long_b.applyPendingIfDue(kBeyondUint32);
  assert(long_a.current().map_version == 11);
  assert(long_b.current().map_version == 11);
  assert(long_a.channelForSequence(kBeyondUint32 + 123ULL) ==
         long_b.channelForSequence(kBeyondUint32 + 123ULL));

  assert(a.rendezvousChannel(0) != a.rendezvousChannel(1));
  assert(a.beaconMatchesSession({42, 1, 0, 2}));
  std::cout << "test_afh: PASS\n";
}
