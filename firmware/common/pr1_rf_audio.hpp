#pragma once
#include <array>
#include <cstdint>
#include <cstring>
#include "pr1_ima_adpcm.hpp"
namespace pr1::audio {
// Payload scrambling (audio layer only; the radio PHY is unchanged). The FLRC link runs
// without whitening, and audio payloads contain long constant runs (a silent block is 94
// zero bytes), which measurably raised CRC errors. TX XORs the 100-byte payload with this
// fixed mask and RX removes it. Mask = 16-bit LFSR (x^16+x^14+x^13+x^11+1), seed 0xACE1.
constexpr std::array<std::uint8_t, kBlockBytes> makeWhitenMask() {
  std::array<std::uint8_t, kBlockBytes> m{};
  std::uint16_t lfsr = 0xACE1U;
  for (std::size_t i = 0; i < kBlockBytes; ++i) {
    std::uint8_t byte = 0;
    for (int b = 0; b < 8; ++b) {
      const std::uint16_t bit = ((lfsr >> 0) ^ (lfsr >> 2) ^ (lfsr >> 3) ^ (lfsr >> 5)) & 1U;
      lfsr = static_cast<std::uint16_t>((lfsr >> 1) | (bit << 15));
      byte = static_cast<std::uint8_t>((byte << 1) | (lfsr & 1U));
    }
    m[i] = byte;
  }
  return m;
}
PR1_AUDIO_DRAM static const std::array<std::uint8_t, kBlockBytes> kWhitenMask = makeWhitenMask();
PR1_AUDIO_IRAM inline void whiten(const std::uint8_t* in, std::uint8_t* out) {
  for (std::size_t i = 0; i < kBlockBytes; ++i) out[i] = static_cast<std::uint8_t>(in[i] ^ kWhitenMask[i]);
}
// Audio time is independent of RF packet sequence / post-transmit gap.
class ClipSource {
 public:
  // repeats = how many times the clip plays after start (0 = loop forever). After that the
  // source keeps sending valid silent blocks, so the link and the playout clock stay up.
  ClipSource(const std::uint8_t* data, std::size_t bytes, std::uint32_t repeats = 0)
      : data_(data), blocks_(bytes/kBlockBytes), repeats_(repeats) {}
  bool finished(std::uint64_t block) const { return repeats_ != 0 && block >= static_cast<std::uint64_t>(blocks_) * repeats_; }
  bool fill(std::uint32_t now, std::uint8_t* out, std::size_t size) {
    if (!blocks_ || size!=kBlockBytes) return false;
    if (!started_) { last_=now; started_=true; }
    elapsed_ += static_cast<std::uint32_t>(now-last_); last_=now;
    const std::uint64_t block=elapsed_/5875U;
    if (finished(block)) std::memset(out,0,kBlockBytes);  // predictor 0, index 0, codes 0 = silence
    else std::memcpy(out,data_+(block%blocks_)*kBlockBytes,kBlockBytes);
    out[0]=static_cast<std::uint8_t>(block); out[1]=static_cast<std::uint8_t>(block>>8);
    whiten(out,out);  // on-air payload is scrambled; Jitter::push() gets the descrambled block
    return true;
  }
 private:
  const std::uint8_t* data_; std::size_t blocks_; std::uint32_t repeats_;
  bool started_=false; std::uint32_t last_=0; std::uint64_t elapsed_=0;
};
// Single consumer owns this buffer; SPSC transport is outside it.
class Jitter {
 public:
  static constexpr unsigned kCapacity=32, kPrefill=6, kLateResync=32;
  bool push(const std::uint8_t* b) {
    if(b[4]>88) { ++invalid; return false; }
    const auto seq=parseHeader(b).seq;
    if(!anchored_) anchor(seq);
    const auto diff=static_cast<std::int16_t>(seq-expected_);
    if(diff<0) {
      // A run of late blocks means the playout point ran ahead of the stream (RX clock
      // faster than TX, or the TX restarted its block counter): re-anchor instead of
      // dropping every block forever.
      ++late;
      if(++late_run_<kLateResync) return false;
      ++resets; anchor(seq);
    } else {
      late_run_=0;
    }
    if(static_cast<std::int16_t>(seq-expected_)>=static_cast<int>(kCapacity)) { ++resets; anchor(seq); }
    auto& slot=slots_[seq%kCapacity];
    if(slot.valid && slot.seq==seq) { ++duplicates; return false; }
    std::memcpy(slot.data.data(),b,kBlockBytes); slot.seq=seq; slot.valid=true;
    if(++queued_>=kPrefill) playing_=true;
    return true;
  }
  bool render(std::int16_t* out) {
    if(!playing_) { std::memset(out,0,kSamplesPerBlock*sizeof(*out)); ++startup; return false; }
    auto& slot=slots_[expected_%kCapacity]; bool ok=slot.valid && slot.seq==expected_;
    if(ok) {
      ok=decodeBlock(slot.data.data(),out); slot.valid=false; --queued_;
      std::memcpy(last_.data(),out,kSamplesPerBlock*sizeof(*out)); ++decoded;
    } else {
      ++missing;
      for(unsigned i=0;i<kSamplesPerBlock;i++) { last_[i]=static_cast<std::int16_t>(last_[i]*3/4); out[i]=last_[i]; }
    }
    ++expected_; return ok;
  }
  std::uint32_t duplicates=0, late=0, invalid=0, resets=0, missing=0, decoded=0, startup=0;
 private:
  struct Slot { std::array<std::uint8_t,kBlockBytes> data{}; std::uint16_t seq=0; bool valid=false; };
  void anchor(std::uint16_t seq) {
    for(auto& slot:slots_) slot.valid=false;
    last_.fill(0); expected_=seq; queued_=0; anchored_=true; playing_=false; late_run_=0;
  }
  std::array<Slot,kCapacity> slots_{}; std::array<std::int16_t,kSamplesPerBlock> last_{};
  std::uint16_t expected_=0; unsigned queued_=0, late_run_=0; bool anchored_=false, playing_=false;
};
}
