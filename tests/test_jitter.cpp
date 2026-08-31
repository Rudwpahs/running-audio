#include <array>
#include <cassert>
#include <iostream>
#include "../firmware/common/pr1_jitter.hpp"
int main() {
  pr1::jitter::Buffer<8> b; b.setAnchor(100,40000);
  std::array<std::uint8_t,100> p{}; p[0]=7;
  assert(b.deadlineFor(100)==40000 && b.deadlineFor(101)==50000);
  assert(b.insert(101,p.data(),p.size(),1000)); assert(b.insert(100,p.data(),p.size(),2000));
  assert(!b.insert(100,p.data(),p.size(),3000));
  pr1::jitter::Frame out{}; assert(b.take(100,39000,&out)); assert(out.payload[0]==7);
  assert(!b.insert(102,p.data(),p.size(),60000));
  assert(pr1::jitter::chooseRecovery({false,true,true,true})==pr1::jitter::RecoveryChoice::XorFec);
  assert(pr1::jitter::chooseRecovery({false,false,false,false})==pr1::jitter::RecoveryChoice::Plc);
  std::cout << "test_jitter: PASS\n";
}
