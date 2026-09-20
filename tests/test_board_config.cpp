#include <cassert>
#include <cstdint>
#include <iostream>

#include "../firmware/t3s3_sx1280_runtime/include/pr1_board_config.hpp"

int main() {
  static_assert(pr1::board::kSx1280SpiHz == 2'000'000U);
  assert(pr1::board::kSx1280SpiHz == 2'000'000U);

  std::cout << "test_board_config: PASS\n";
  return 0;
}
