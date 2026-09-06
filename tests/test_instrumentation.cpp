#include <cassert>
#include <iostream>
#include "../firmware/common/pr1_instrumentation.hpp"
int main() {
  pr1::instrumentation::TraceRing<3> ring;
  ring.push({pr1::instrumentation::Event::IsrEnter, 10, 1, 0});
  ring.push({pr1::instrumentation::Event::SpiReadStart, 20, 1, 0});
  ring.push({pr1::instrumentation::Event::SpiReadEnd, 30, 1, 0});
  ring.push({pr1::instrumentation::Event::AudioPlayed, 40, 1, 0});
  assert(ring.size() == 3 && ring.overwrites() == 1);
  pr1::instrumentation::TraceEntry e{};
  assert(ring.atOldest(0, &e) && e.timestamp_us == 20);

  ring.push({pr1::instrumentation::Event::RxRearmDone, 50, 2, 125});
  assert(ring.atOldest(2, &e));
  assert(e.event == pr1::instrumentation::Event::RxRearmDone);
  assert(e.timestamp_us == 50);
  assert(e.sequence == 2);
  assert(e.value == 125);

  pr1::instrumentation::DurationWindow<8> w;
  for (std::uint32_t v : {1U,2U,3U,4U,5U,6U,7U,100U}) w.observe(v);
  assert(w.percentile(50) == 5);
  assert(w.percentile(95) == 100);
  assert(w.maxUs() == 100);
  std::cout << "test_instrumentation: PASS\n";
}
