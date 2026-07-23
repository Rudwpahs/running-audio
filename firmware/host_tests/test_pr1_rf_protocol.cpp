#include <cassert>
#include <cstdint>
#include <iostream>

#include "../common/pr1_rf_protocol.h"

int main() {
  using namespace pr1::rf;

  static_assert(kTransportHeaderSize == 10);

  uint8_t raw[kTransportHeaderSize]{};
  const TransportHeader source{
      3,      // flags
      65535,  // packet_seq
      1234,   // frame_seq
      2,      // fragment_index
      4,      // fragment_count
  };

  assert(EncodeTransportHeader(source, raw, sizeof(raw)) == Error::kOk);

  TransportHeader decoded{};
  assert(DecodeTransportHeader(raw, sizeof(raw), &decoded) == Error::kOk);
  assert(decoded.flags == source.flags);
  assert(decoded.packet_seq == source.packet_seq);
  assert(decoded.frame_seq == source.frame_seq);
  assert(decoded.fragment_index == source.fragment_index);
  assert(decoded.fragment_count == source.fragment_count);

  // Canonical voice v0 application packet on current main:
  // 16-byte app header + 160-byte PCM = 176 bytes. The lower RF transport
  // can carry it in one 255-byte packet or fragment it into two 127-byte
  // packets without changing the application codec.
  assert(FragmentCount(176, 255) == 1);
  assert(FragmentCount(176, 127) == 2);

  // Regression check for the older 640-byte raw PCM application frame.
  assert(FragmentCount(640, 127) == 6);
  assert(FragmentCount(640, 255) == 3);

  std::size_t offset = 0;
  std::size_t length = 0;
  assert(FragmentBounds(640, 127, 5, &offset, &length) == Error::kOk);
  assert(offset == 585);
  assert(length == 55);

  std::cout << "PR1 C++ RF transport host tests PASS\n";
  return 0;
}
