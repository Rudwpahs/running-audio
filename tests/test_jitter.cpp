#include <array>
#include <cassert>
#include <cstdint>
#include <iostream>

#include "../firmware/common/pr1_jitter.hpp"
#include "../firmware/common/pr1_sequence.hpp"

int main() {
  using pr1::sequence::LogicalFrameId;

  pr1::jitter::Buffer<8> b;
  b.setAnchor(LogicalFrameId{1, 100}, 40000ULL);
  std::array<std::uint8_t, 100> p{};
  p[0] = 7;
  assert(b.deadlineFor(LogicalFrameId{1, 100}) == 40000ULL);
  assert(b.deadlineFor(LogicalFrameId{1, 101}) == 50000ULL);
  assert(b.insert(LogicalFrameId{1, 101}, p.data(), p.size(), 1000ULL));
  assert(b.insert(LogicalFrameId{1, 100}, p.data(), p.size(), 2000ULL));
  assert(!b.insert(LogicalFrameId{1, 100}, p.data(), p.size(), 3000ULL));
  pr1::jitter::Frame out{};
  assert(b.take(LogicalFrameId{1, 100}, 39000ULL, &out));
  assert(out.payload[0] == 7);
  assert(!b.insert(LogicalFrameId{1, 102}, p.data(), p.size(), 60000ULL));

  pr1::jitter::Buffer<8> long_session;
  constexpr std::uint64_t kAnchorUs = 1000000ULL;
  long_session.setAnchor(LogicalFrameId{7, 0}, kAnchorUs);
  assert(long_session.deadlineFor(LogicalFrameId{7, 32767}) ==
         kAnchorUs + 32767ULL * 10000ULL);
  assert(long_session.deadlineFor(LogicalFrameId{7, 32768}) ==
         kAnchorUs + 32768ULL * 10000ULL);
  assert(long_session.deadlineFor(LogicalFrameId{7, 65536}) ==
         kAnchorUs + 65536ULL * 10000ULL);
  assert(long_session.deadlineFor(LogicalFrameId{7, 90000}) ==
         kAnchorUs + 90000ULL * 10000ULL);
  assert(long_session.deadlineFor(LogicalFrameId{7, 720000}) ==
         kAnchorUs + 720000ULL * 10000ULL);

  // Same low 16-bit wire sequence, different logical frame: not a duplicate.
  assert(long_session.insert(LogicalFrameId{7, 0}, p.data(), p.size(), 100ULL));
  assert(long_session.insert(LogicalFrameId{7, 65536}, p.data(), p.size(), 200ULL));

  // A prior session generation is never accepted into the current buffer.
  assert(!long_session.insert(LogicalFrameId{6, 65537}, p.data(), p.size(), 300ULL));

  assert(pr1::jitter::chooseRecovery({false, true, true, true}) ==
         pr1::jitter::RecoveryChoice::XorFec);
  assert(pr1::jitter::chooseRecovery({false, false, false, false}) ==
         pr1::jitter::RecoveryChoice::Plc);
  std::cout << "test_jitter: PASS\n";
}
