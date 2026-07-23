#include <cassert>
#include <cstdint>
#include <iostream>

#include "../common/pr1_rf_protocol.h"

int main() {
  using namespace pr1;

  static_assert(kHeaderSize == 10);

  uint8_t raw[kHeaderSize]{};
  const Header source{
      3,      // flags
      65535,  // packet_seq
      1234,   // frame_seq
      2,      // fragment_index
      4,      // fragment_count
  };

  assert(EncodeHeader(source, raw, sizeof(raw)) == Error::kOk);

  Header decoded{};
  assert(DecodeHeader(raw, sizeof(raw), &decoded) == Error::kOk);
  assert(decoded.flags == source.flags);
  assert(decoded.packet_seq == source.packet_seq);
  assert(decoded.frame_seq == source.frame_seq);
  assert(decoded.fragment_index == source.fragment_index);
  assert(decoded.fragment_count == source.fragment_count);

  // 10-byte PR1 header leaves 117 bytes in a 127-byte FLRC packet.
  assert(FragmentCount(640, 127) == 6);

  // 10-byte PR1 header leaves 245 bytes in a 255-byte GFSK packet.
  assert(FragmentCount(640, 255) == 3);

  // The current low-complexity codec candidate is ~84 bytes / 10 ms.
  assert(FragmentCount(84, 127) == 1);

  std::size_t offset = 0;
  std::size_t length = 0;
  assert(FragmentBounds(640, 127, 5, &offset, &length) == Error::kOk);
  assert(offset == 585);
  assert(length == 55);

  std::cout << "PR1 C++ protocol host tests PASS\n";
  return 0;
}
