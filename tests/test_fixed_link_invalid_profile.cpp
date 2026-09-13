#include <cassert>
#include <cstddef>
#include <cstdint>
#include <iostream>

#include "../firmware/t3s3_sx1280_runtime/include/pr1_fixed_link_runtime.hpp"
#include "../firmware/t3s3_sx1280_runtime/include/pr1_radio_port.hpp"

namespace {

class NullRadio final : public pr1::runtime::RadioPort {
 public:
  bool beginFixedFlrc(const pr1::runtime::FixedFlrcProfile&) override {
    ++begin_calls;
    return true;
  }
  bool startReceive() override { return true; }
  pr1::runtime::RadioReadResult readPacket(std::uint8_t*, std::size_t, std::size_t*) override {
    return pr1::runtime::RadioReadResult::Error;
  }
  bool transmit(const std::uint8_t*, std::size_t) override { return true; }
  std::int16_t rssiDbm() override { return -40; }
  std::int16_t snrDb() override { return 0; }
  std::uint32_t nowMicros() const override { return 0; }
  void setRxIrqHandler(pr1::runtime::RxIrqHandler, void*) override {}

  unsigned begin_calls = 0;
};

}  // namespace

int main() {
  NullRadio radio;
  auto invalid = pr1::runtime::kFixedFlrcProfile;
  invalid.tx_period_us = 0U;

  pr1::runtime::FixedLinkRuntime runtime(
      radio, pr1::runtime::RuntimeRole::Tx, invalid, 1U);

  assert(!runtime.begin());
  assert(radio.begin_calls == 0U);

  std::cout << "test_fixed_link_invalid_profile: PASS\n";
  return 0;
}
