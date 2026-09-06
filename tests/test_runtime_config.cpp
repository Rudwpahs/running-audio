#include <cassert>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <string_view>

#include "../firmware/t3s3_sx1280_runtime/include/pr1_runtime_config.hpp"

int main() {
  using namespace pr1::runtime;

  static_assert(!kBootMetadata.rf_enabled, "Round 2 must default to RF disabled");
  static_assert(!kBootMetadata.hardware_verified,
                "CI must not claim physical hardware verification");
  static_assert(kBootMetadata.protocol_version == 1, "Unexpected PR1 protocol version");
  static_assert(kBootMetadata.protocol_header_bytes == 16,
                "Unexpected PR1 protocol header length");

  static_assert(kBootMetadata.pins.cs == 7);
  static_assert(kBootMetadata.pins.rst == 8);
  static_assert(kBootMetadata.pins.sclk == 5);
  static_assert(kBootMetadata.pins.mosi == 6);
  static_assert(kBootMetadata.pins.miso == 3);
  static_assert(kBootMetadata.pins.dio1 == 9);
  static_assert(kBootMetadata.pins.busy == 36);
  static_assert(kBootMetadata.pins.tx_enable == 10);
  static_assert(kBootMetadata.pins.rx_enable == 21);

  assert(std::string_view{kBootMetadata.board_family} == "LILYGO T3-S3-MVSRBoard");
  assert(std::string_view{kBootMetadata.radio_target} == "SX1280");
  std::cout << "test_runtime_config: PASS\n";
  return 0;
}
