#include <cassert>
#include <iostream>
#include "../firmware/common/pr1_channel_quality.hpp"
int main() {
  pr1::quality::Config cfg{}; cfg.suspect_losses = 1; cfg.exclude_losses = 2;
  pr1::quality::Estimator q(cfg);
  q.observeData(3, false, 0);
  assert(q.channel(3).state == pr1::quality::ChannelState::Suspect);
  q.observeData(3, false, 10);
  assert(q.channel(3).state == pr1::quality::ChannelState::Excluded);
  assert(q.activeCount() == 39);
  assert(!q.probeDue(3, 100));
  assert(q.probeDue(3, 210));
  q.beginProbe(3, 210); q.observeProbe(3, true, 210);
  q.beginProbe(3, 410); q.observeProbe(3, true, 410);
  q.beginProbe(3, 610); q.observeProbe(3, false, 610);
  assert(q.channel(3).state == pr1::quality::ChannelState::Active);
  assert(q.activeCount() == 40);
  auto map = q.activeMap(); assert(map.activeCount() == 40);
  std::cout << "test_channel_quality: PASS\n";
}
