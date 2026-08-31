#include <array>
#include <cassert>
#include <cstdint>
#include <iostream>

#include "../firmware/common/pr1_fec.hpp"

using P = std::array<std::uint8_t, pr1::fec::kCodecPayloadBytes>;

static P makePayload(std::uint8_t salt) {
  P p{};
  for (std::size_t i = 0; i < p.size(); ++i) {
    p[i] = static_cast<std::uint8_t>(i + static_cast<std::size_t>(salt) * 17U);
  }
  return p;
}

int main() {
  {
    std::array<P, 4> p{{makePayload(0), makePayload(1), makePayload(2), makePayload(3)}};
    std::array<const std::uint8_t*, 4> src{{p[0].data(), p[1].data(), p[2].data(), p[3].data()}};
    pr1::fec::ParityFrame<4> parity{};
    assert(pr1::fec::encode<4>(99, src, &parity));

    pr1::fec::Stats stats{};
    src[2] = nullptr;
    std::uint8_t missing = 255;
    P recovered{};
    assert(pr1::fec::recoverOneDetailed<4>(parity, src, &missing, &recovered, &stats) ==
           pr1::fec::RecoveryStatus::Recovered);
    assert(missing == 2);
    assert(recovered == p[2]);
    assert(stats.recovered == 1);

    src[1] = nullptr;
    assert(pr1::fec::recoverOneDetailed<4>(parity, src, &missing, &recovered, &stats) ==
           pr1::fec::RecoveryStatus::TooManyMissing);
    assert(stats.unrecoverable == 1);
  }

  // 3+1 severe-link experiment uses the exact same codec-payload XOR core.
  {
    std::array<P, 3> p{{makePayload(4), makePayload(5), makePayload(6)}};
    std::array<const std::uint8_t*, 3> src{{p[0].data(), p[1].data(), p[2].data()}};
    pr1::fec::ParityFrame<3> parity{};
    assert(pr1::fec::encode<3>(123, src, &parity));
    src[0] = nullptr;
    std::uint8_t missing = 255;
    P recovered{};
    assert(pr1::fec::recoverOne<3>(parity, src, &missing, &recovered));
    assert(missing == 0);
    assert(recovered == p[0]);
  }

  // Reject ambiguous/false recovery cases explicitly.
  {
    std::array<P, 4> p{{makePayload(7), makePayload(8), makePayload(9), makePayload(10)}};
    std::array<const std::uint8_t*, 4> src{{p[0].data(), p[1].data(), p[2].data(), p[3].data()}};
    pr1::fec::ParityFrame<4> parity{};
    assert(pr1::fec::encode<4>(7, src, &parity));
    std::uint8_t missing = 255;
    P recovered{};
    pr1::fec::Stats stats{};
    assert(pr1::fec::recoverOneDetailed<4>(parity, src, &missing, &recovered, &stats) ==
           pr1::fec::RecoveryStatus::NoMissing);
    parity.source_bitmap = 0x07U;
    assert(pr1::fec::recoverOneDetailed<4>(parity, src, &missing, &recovered, &stats) ==
           pr1::fec::RecoveryStatus::InvalidMetadata);
    parity.source_bitmap = pr1::fec::expectedBitmap<4>();
    assert(pr1::fec::recoverOneForGroup<4>(8, parity, src, &missing, &recovered, &stats) ==
           pr1::fec::RecoveryStatus::InvalidMetadata);
    assert(stats.rejected_ambiguous == 3);
  }

  // Wire payload is 4 bytes metadata + 100 bytes parity and round-trips.
  {
    std::array<P, 4> p{{makePayload(11), makePayload(12), makePayload(13), makePayload(14)}};
    std::array<const std::uint8_t*, 4> src{{p[0].data(), p[1].data(), p[2].data(), p[3].data()}};
    pr1::fec::ParityFrame<4> parity{};
    assert(pr1::fec::encode<4>(0xBEEF, src, &parity));
    std::array<std::uint8_t, pr1::fec::kParityPayloadBytes> bytes{};
    assert(pr1::fec::encodeParityPayload(parity, &bytes));
    pr1::fec::ParityFrame<4> decoded{};
    assert(pr1::fec::decodeParityPayload<4>(bytes.data(), bytes.size(), &decoded));
    assert(decoded.group_id == 0xBEEF);
    assert(decoded.parity == parity.parity);
    bytes[2] = 3;
    assert(!pr1::fec::decodeParityPayload<4>(bytes.data(), bytes.size(), &decoded));
  }

  // Streaming encoder emits parity exactly after the fourth source and then
  // starts the next group immediately.
  {
    pr1::fec::Stats stats{};
    pr1::fec::StreamingEncoder<4> encoder(200, &stats);
    pr1::fec::ParityFrame<4> parity{};
    P a = makePayload(1), b = makePayload(2), c = makePayload(3), d = makePayload(4);
    assert(!encoder.push(a.data(), &parity));
    assert(!encoder.push(b.data(), &parity));
    assert(!encoder.push(c.data(), &parity));
    assert(encoder.push(d.data(), &parity));
    assert(parity.group_id == 200);
    assert(stats.parity_sent == 1);
    assert(encoder.currentGroupId() == 201);
    assert(encoder.sourceIndex() == 0);
    assert(!encoder.push(a.data(), &parity));
    assert(encoder.sourceIndex() == 1);
    encoder.reset(300);
    assert(encoder.currentGroupId() == 300);
    assert(encoder.sourceIndex() == 0);
  }

  // Repair channel must differ from primary when >=2 ACTIVE channels exist.
  {
    pr1::afh::ChannelMap map{};
    std::uint8_t repair = 255;
    assert(pr1::fec::chooseRepairChannel(map, 10, 0, &repair));
    assert(repair != 10);
    assert(map.isActive(repair));

    map.bits = (1ULL << 10);
    assert(!pr1::fec::chooseRepairChannel(map, 10, 0, &repair));
  }

  // Runtime policy exposes OFF / 4+1 / 3+1 without compile-time rewiring.
  {
    pr1::fec::Policy p{};
    assert(!p.enabled());
    assert(p.sourceCount() == 0);
    p.mode = pr1::fec::Mode::Xor4Plus1;
    assert(p.enabled() && p.sourceCount() == 4);
    p.mode = pr1::fec::Mode::Xor3Plus1;
    assert(p.enabled() && p.sourceCount() == 3);
    assert(!p.interleave_depth2);
  }

  std::cout << "test_fec: PASS\n";
}
