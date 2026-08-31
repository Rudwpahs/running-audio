#include <array>
#include <cassert>
#include <cstdint>
#include <iostream>
#include "../firmware/common/pr1_fec.hpp"
int main() {
  using P = std::array<std::uint8_t, pr1::fec::kCodecPayloadBytes>;
  std::array<P,4> p{};
  for (std::size_t s=0;s<4;++s) for(std::size_t i=0;i<p[s].size();++i) p[s][i]=static_cast<std::uint8_t>(i+s*17);
  std::array<const std::uint8_t*,4> src{{p[0].data(),p[1].data(),p[2].data(),p[3].data()}};
  pr1::fec::ParityFrame<4> parity{}; assert(pr1::fec::encode<4>(99,src,&parity));
  src[2]=nullptr; std::uint8_t missing=255; P recovered{};
  assert(pr1::fec::recoverOne<4>(parity,src,&missing,&recovered)); assert(missing==2); assert(recovered==p[2]);
  src[1]=nullptr; assert(!pr1::fec::recoverOne<4>(parity,src,&missing,&recovered));
  std::cout << "test_fec: PASS\n";
}
