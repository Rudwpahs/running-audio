#include <cassert>
#include <iostream>
#include "../firmware/common/pr1_phy.hpp"
int main() {
  assert(pr1::phy::estimateAirtimeUs(pr1::phy::ProfileId::Flrc1300Cr34,116) < pr1::phy::estimateAirtimeUs(pr1::phy::ProfileId::Flrc650Cr34,116));
  assert(pr1::phy::classify({20,300,150,0})==pr1::phy::LinkClass::Interference);
  assert(pr1::phy::classify({50,700,50,0})==pr1::phy::LinkClass::WeakLink);
  assert(pr1::phy::classify({5,0,200,2})==pr1::phy::LinkClass::Burst);
  pr1::phy::Ladder ladder(1000); assert(ladder.current()==pr1::phy::ProfileId::Flrc1300Cr34);
  assert(ladder.update(pr1::phy::LinkClass::WeakLink,1000)); assert(ladder.current()==pr1::phy::ProfileId::Flrc650Cr34);
  assert(!ladder.update(pr1::phy::LinkClass::WeakLink,1500)); assert(ladder.update(pr1::phy::LinkClass::WeakLink,2000));
  assert(ladder.current()==pr1::phy::ProfileId::Flrc520Cr34);
  std::cout << "test_phy: PASS\n";
}
