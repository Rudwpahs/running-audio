#include <cassert>
#include <array>
#include <iostream>
#include "../firmware/common/pr1_rf_audio.hpp"
using namespace pr1::audio;
std::array<std::uint8_t,100> block(std::uint16_t s) {
 std::array<std::uint8_t,100> b{}; b[0]=s; b[1]=s>>8; return b;
}
// ClipSource emits the scrambled on-air payload; tests look at the descrambled block.
struct Plain {
 ClipSource& src;
 bool fill(std::uint32_t now,std::uint8_t* out,std::size_t n) {
  if(!src.fill(now,out,n)) return false;
  whiten(out,out);
  return true;
 }
};
int main() {
 {
  // Scrambling: involution, and a silent block has no long constant run on air.
  std::array<std::uint8_t,100> z{}, w{}, back{}; whiten(z.data(),w.data()); whiten(w.data(),back.data());
  assert(back==z); unsigned run=0,maxrun=0;
  for(unsigned i=1;i<100;i++){ run=(w[i]==w[i-1])?run+1:0; if(run>maxrun) maxrun=run; }
  assert(maxrun<3); unsigned ones=0; for(auto v:w) ones+=__builtin_popcount(v);
  assert(ones>300 && ones<500);
 }
 std::array<std::uint8_t,200> clip{}; clip[100+2]=123;
 ClipSource source_raw(clip.data(),clip.size()); Plain source{source_raw}; std::array<std::uint8_t,100> out{};
 assert(source.fill(0xfffffff0U,out.data(),100)); assert(out[0]==0);
 assert(source.fill(0xfffffff0U+5874U,out.data(),100)); assert(out[0]==0);
 assert(source.fill(0xfffffff0U+5875U,out.data(),100)); assert(out[0]==1 && out[2]==123);
 assert(source.fill(0xfffffff0U+11750U,out.data(),100)); assert(out[0]==2 && out[2]==0);
 {
  // Finite repeats: after N plays the source sends silent, decodable blocks with running seq.
  std::array<std::uint8_t,200> c2{}; c2[2]=55; c2[100+2]=66;
  ClipSource twice_raw(c2.data(),c2.size(),2); Plain twice{twice_raw}; std::array<std::uint8_t,100> o{};
  assert(twice.fill(0,o.data(),100) && o[2]==55);
  assert(twice.fill(3*5875U,o.data(),100) && o[0]==3 && o[2]==66);   // second play
  assert(twice.fill(4*5875U,o.data(),100) && o[0]==4 && o[2]==0 && o[4]==0);
  std::array<std::int16_t,188> z{}; z.fill(7); assert(decodeBlock(o.data(),z.data()));
  for(auto v:z) assert(v==0);
 }
 Jitter j; auto b=block(65534); assert(j.push(b.data())); assert(!j.push(b.data()));
 for(unsigned i=1;i<6;i++){ b=block(65534+i); assert(j.push(b.data())); }
 std::array<std::int16_t,188> pcm{}; assert(j.render(pcm.data()));
 for(unsigned i=0;i<5;i++) assert(j.render(pcm.data()));
 assert(!j.render(pcm.data())); assert(j.missing==1 && j.duplicates==1);
 b=block(6); assert(j.push(b.data())); assert(!j.render(pcm.data()));
 assert(j.render(pcm.data())); // skipped seq5, then seq6
 b=block(6); b[4]=89; assert(!j.push(b.data())); assert(j.invalid==1);
 b=block(200); assert(j.push(b.data())); assert(j.resets==1);
 assert(!j.render(pcm.data())); // rebuffer after discontinuity
 {
  // RX clock faster than TX: the playout point runs ahead, every block is late.
  // After kLateResync late blocks the buffer re-anchors and plays again (no permanent mute).
  Jitter d; std::uint16_t s=1000;
  for(unsigned i=0;i<6;i++){ b=block(s++); assert(d.push(b.data())); }
  for(unsigned i=0;i<40;i++) d.render(pcm.data());          // playout runs far ahead
  unsigned accepted=0;
  for(unsigned i=0;i<Jitter::kLateResync+8;i++){ b=block(s++); accepted+=d.push(b.data()); }
  assert(d.resets==1 && accepted==9);                        // re-anchored on the 32nd late block
  for(unsigned i=0;i<6;i++){ b=block(s++); d.push(b.data()); }
  assert(d.render(pcm.data()));
  // TX restart: block counter back to 0 while RX expects ~1050.
  Jitter t; s=1000;
  for(unsigned i=0;i<8;i++){ b=block(s++); assert(t.push(b.data())); }
  for(unsigned i=0;i<8;i++) assert(t.render(pcm.data()));
  s=0; unsigned ok=0;
  for(unsigned i=0;i<Jitter::kLateResync+6;i++){ b=block(s++); ok+=t.push(b.data()); }
  assert(t.resets==1 && ok==7);
  assert(t.render(pcm.data()));
  // An isolated late block does not re-anchor.
  Jitter n; s=50;
  for(unsigned i=0;i<6;i++){ b=block(s++); n.push(b.data()); }
  n.render(pcm.data()); b=block(50); assert(!n.push(b.data())); assert(n.late==1 && n.resets==0);
 }
 {
  // Time-diverse repetition: alternate newest block / block from 4 blocks earlier.
  std::array<std::uint8_t,100*16> c3{}; for(unsigned k=0;k<16;k++) c3[k*100+2]=static_cast<std::uint8_t>(k+1);
  ClipSource div_raw(c3.data(),c3.size(),0,4); Plain div{div_raw}; std::array<std::uint8_t,100> o{};
  unsigned seen[16]={};
  for(unsigned n=0;n<24;n++) {               // two packets per block period
    assert(div.fill(n*2938U,o.data(),100)); assert(o[2]==o[0]+1); ++seen[o[0]];
  }
  for(unsigned k=4;k<8;k++) assert(seen[k]==2);  // newest copy + the copy 4 blocks later
  for(unsigned k=0;k<4;k++) assert(seen[k]==3);  // start-up: no older block exists yet
  // Concealment: a missing block ramps to zero without a step and the next block fades in.
  Jitter pl; std::uint16_t s2=10; std::array<std::int16_t,188> p{};
  for(unsigned i=0;i<6;i++){ auto bb=block(s2++); bb[2]=0x10; bb[3]=0x27; pl.push(bb.data()); }  // predictor 10000
  for(unsigned i=0;i<6;i++) assert(pl.render(p.data()));
  const std::int16_t last=p[187]; assert(!pl.render(p.data()));
  assert(p[0]==last && p[Jitter::kFade]==0 && p[187]==0);
  auto bb=block(s2+1); bb[2]=0x10; bb[3]=0x27; pl.push(bb.data());  // s2 was lost, s2+1 arrives
  assert(pl.render(p.data())); assert(p[0]==0 && p[187]!=0);    // fades in from zero
 }
 {
  // Manual start: silent (valid, decodable) blocks with a running seq until play(); a reset never plays.
  std::array<std::uint8_t,100*8> c4{}; for(unsigned k=0;k<8;k++) c4[k*100+2]=static_cast<std::uint8_t>(k+1);
  ClipSource man_raw(c4.data(),c4.size(),1,0,true); Plain man{man_raw}; std::array<std::uint8_t,100> o{};
  for(unsigned n=0;n<6;n++) { assert(man.fill(n*5875U,o.data(),100)); assert(o[0]==n && o[2]==0); }
  man_raw.play();
  assert(man.fill(6*5875U,o.data(),100) && o[0]==6 && o[2]==1);        // clip starts at its first block
  assert(man.fill(7*5875U,o.data(),100) && o[0]==7 && o[2]==2);
  assert(man.fill(13*5875U,o.data(),100) && o[0]==13 && o[2]==8);      // last block
  assert(man.fill(14*5875U,o.data(),100) && o[0]==14 && o[2]==0);      // one play, then silence again
  assert(man_raw.plays==1);
  man_raw.play(); assert(man.fill(15*5875U,o.data(),100) && o[0]==15 && o[2]==1 && man_raw.plays==2);
  // Time-diverse copy never reaches back before the clip start.
  ClipSource md_raw(c4.data(),c4.size(),1,4,true); Plain md{md_raw};
  for(unsigned n=0;n<10;n++) md.fill(n*2938U,o.data(),100);             // silent lead-in
  md_raw.play();
  for(unsigned n=10;n<20;n++) { assert(md.fill(n*2938U,o.data(),100)); assert(o[2]==o[0]-(10*2938U/5875U)+1 || o[2]==o[0]-(10*2938U/5875U)+1-4); }
 }
 std::cout << "test_rf_audio: PASS\n";
}
