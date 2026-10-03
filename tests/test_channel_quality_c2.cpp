// Gate C2 estimator configuration (issue #52): the replay-selected values.
#include <cassert>
#include <cstdint>
#include <iostream>

#include "../firmware/common/pr1_channel_quality.hpp"

using pr1::quality::ChannelState;
using pr1::quality::Config;
using pr1::quality::Estimator;

namespace {

Config c2() {
  Config c{};
  c.exclude_pdr_q15 = 22937;       // 0.70
  c.exclude_slow_pdr_q15 = 31129;  // 0.95
  c.reinstate_probe_successes = 3;
  c.initial_probe_ms = 3200;
  c.max_probe_ms = 25600;
  return c;
}

void defaultsUnchanged() {
  // Gate C defaults: one loss then one success excludes (the contradiction C2 removes).
  Estimator e{};
  for (int i = 0; i < 20; ++i) e.observeData(5, true, 0);
  e.observeData(5, false, 10);
  assert(e.channel(5).state == ChannelState::Suspect);
  e.observeData(5, true, 20);
  assert(e.channel(5).state == ChannelState::Excluded);
}

void isolatedLossNeverExcludes() {
  Estimator e{c2()};
  std::uint32_t t = 0;
  for (int rep = 0; rep < 50; ++rep) {
    e.observeData(7, false, t += 3);  // one loss ...
    for (int i = 0; i < 40; ++i) e.observeData(7, true, t += 3);  // ... then clean visits
    assert(e.channel(7).state != ChannelState::Excluded);
  }
}

void repeatedLossExcludes() {
  Estimator e{c2()};
  std::uint32_t t = 0;
  for (int i = 0; i < 20; ++i) e.observeData(9, true, t += 3);
  e.observeData(9, false, t += 3);
  e.observeData(9, true, t += 3);
  e.observeData(9, false, t += 3);  // 2 losses within 3 visits: fast < 0.70
  e.observeData(9, true, t += 3);
  assert(e.channel(9).state == ChannelState::Excluded);

  // Persistent moderate loss (1 in 6 visits) is caught by the slow EWMA.
  Estimator s{c2()};
  bool excluded = false;
  for (int i = 0; i < 120 && !excluded; ++i) {
    s.observeData(11, i % 6 != 5, t += 3);
    excluded = s.channel(11).state == ChannelState::Excluded;
  }
  assert(excluded);
}

void reinclusionNeedsThreeOfThree() {
  Estimator e{c2()};
  std::uint32_t t = 0;
  for (int i = 0; i < 4; ++i) e.observeData(3, false, t += 3);  // 4 in a row
  assert(e.channel(3).state == ChannelState::Excluded);
  const bool pattern[] = {true, false, true, true};  // 2 of 3 is not enough, then 3 of 3
  for (bool ok : pattern) {
    t += 30000;
    assert(e.beginProbe(3, t));
    e.observeProbe(3, ok, t);
  }
  // history after the 4 probes: (false, true, true) -> still excluded
  assert(e.channel(3).state == ChannelState::Excluded);
  t += 30000;
  assert(e.beginProbe(3, t));
  e.observeProbe(3, true, t);
  assert(e.channel(3).state == ChannelState::Active);
}

void probeIntervalRespected() {
  Estimator e{c2()};
  std::uint32_t t = 1000;
  for (int i = 0; i < 4; ++i) e.observeData(2, false, t);
  assert(!e.beginProbe(2, t + 3199));
  assert(e.beginProbe(2, t + 3200));
}

}  // namespace

int main() {
  defaultsUnchanged();
  isolatedLossNeverExcludes();
  repeatedLossExcludes();
  reinclusionNeedsThreeOfThree();
  probeIntervalRespected();
  std::cout << "test_channel_quality_c2: PASS\n";
  return 0;
}
