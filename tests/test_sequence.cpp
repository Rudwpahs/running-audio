#include <cassert>
#include <cstdint>
#include <iostream>

#include "../firmware/common/pr1_sequence.hpp"

int main() {
  using pr1::sequence::SequenceUnwrapper;
  using pr1::sequence::UnwrapStatus;

  SequenceUnwrapper uninitialized;
  assert(uninitialized.preview(1).status == UnwrapStatus::Uninitialized);

  SequenceUnwrapper origin;
  origin.reset(0, 0);
  const auto before_origin = origin.preview(65535);
  assert(before_origin.status == UnwrapStatus::BeforeOrigin);
  assert(origin.latest() == 0);
  assert(origin.rawReference() == 0);

  const auto half = origin.preview(32768);
  assert(half.status == UnwrapStatus::AmbiguousHalfRange);
  assert(!origin.acceptForward(half));
  assert(origin.latest() == 0);
  assert(origin.rawReference() == 0);

  SequenceUnwrapper wrap;
  wrap.reset(65534, 65534);
  const auto last = wrap.preview(65535);
  assert(last.status == UnwrapStatus::Ok);
  assert(last.index == 65535);
  assert(last.forward);
  assert(wrap.acceptForward(last));

  const auto first_wrapped = wrap.preview(0);
  assert(first_wrapped.status == UnwrapStatus::Ok);
  assert(first_wrapped.index == 65536);
  assert(first_wrapped.forward);
  assert(wrap.acceptForward(first_wrapped));

  const auto reordered = wrap.preview(65535);
  assert(reordered.status == UnwrapStatus::Ok);
  assert(reordered.index == 65535);
  assert(!reordered.forward);
  assert(!wrap.acceptForward(reordered));
  assert(wrap.latest() == 65536);

  SequenceUnwrapper soak;
  soak.reset(0, 0);
  constexpr std::uint64_t kFrames = 4ULL * 65536ULL + 1234ULL;
  for (std::uint64_t logical = 1; logical <= kFrames; ++logical) {
    const auto raw = static_cast<std::uint16_t>(logical & 0xFFFFULL);
    const auto candidate = soak.preview(raw);
    assert(candidate.status == UnwrapStatus::Ok);
    assert(candidate.index == logical);
    assert(candidate.forward);
    assert(soak.acceptForward(candidate));
  }
  assert(soak.latest() == kFrames);
  assert(soak.rawReference() == static_cast<std::uint16_t>(kFrames & 0xFFFFULL));

  std::cout << "test_sequence: PASS\n";
}
