#include <cassert>
#include <iostream>
#include "../firmware/common/pr1_link_controller.hpp"
int main() {
  pr1::controller::LinkController c;
  pr1::controller::Metrics m{}; c.update(m,0); assert(c.state()==pr1::controller::State::Good);
  m.bad_channel_permille=300; m.rssi_margin_db_x10=150; c.update(m,1000); assert(c.state()==pr1::controller::State::Interference); assert(c.actions().aggressive_probe);
  m={}; m.per_1s_permille=60; m.rssi_margin_db_x10=50; c.update(m,2000); assert(c.state()==pr1::controller::State::WeakLink); assert(c.actions().xor_fec);
  m={}; m.burst_max=3; c.update(m,3000); assert(c.state()==pr1::controller::State::Burst); assert(c.actions().phy_profile==pr1::phy::ProfileId::Flrc520Cr34);
  m={}; c.update(m,4000); assert(c.state()==pr1::controller::State::Recovery); c.update(m,5500); assert(c.state()==pr1::controller::State::Recovery); c.update(m,6000); assert(c.state()==pr1::controller::State::Good);
  m={}; m.airtime_percent=80; c.update(m,7000); assert(!c.actions().deadline_arq);
  assert(c.transitionCount()>=5);
  std::cout << "test_link_controller: PASS\n";
}
