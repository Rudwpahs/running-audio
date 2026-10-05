#pragma once
#include <array>
#include <atomic>
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
  // delay_blocks > 0 enables time-diverse repetition: every other packet carries the block
  // from delay_blocks earlier instead of the newest one, so the two copies of a block are
  // ~delay_blocks * 5.9 ms apart and one interference burst rarely removes both. Needs about
  // two packets per block (gap 150 us) and delay_blocks < Jitter::kPrefill.
  // manual_start = true: the source sends silent (valid) blocks until play() is called, so a
  // power-up or reset never makes sound by itself. play() may be called from another core.
  ClipSource(const std::uint8_t* data, std::size_t bytes, std::uint32_t repeats = 0, std::uint32_t delay_blocks = 0,
             bool manual_start = false)
      : data_(data), blocks_(bytes/kBlockBytes), repeats_(repeats), delay_(delay_blocks),
        playing_(!manual_start) {}
  void play() { play_req_.store(true, std::memory_order_release); }
  bool finished(std::uint64_t block) const {
    return !playing_ || (repeats_ != 0 && block - start_ >= static_cast<std::uint64_t>(blocks_) * repeats_);
  }
  bool fill(std::uint32_t now, std::uint8_t* out, std::size_t size) {
    if (!blocks_ || size!=kBlockBytes) return false;
    if (!started_) { last_=now; started_=true; }
    elapsed_ += static_cast<std::uint32_t>(now-last_); last_=now;
    std::uint64_t block=elapsed_/5875U;
    if (play_req_.load(std::memory_order_acquire) && play_req_.exchange(false)) {
      start_=block; playing_=true; ++plays;   // the clip restarts at its first block
    }
    older_ = !older_;
    if (delay_ != 0 && older_ && block >= start_ + delay_) block -= delay_;
    if (finished(block)) std::memset(out,0,kBlockBytes);  // predictor 0, index 0, codes 0 = silence
    else std::memcpy(out,data_+((block-start_)%blocks_)*kBlockBytes,kBlockBytes);
    out[0]=static_cast<std::uint8_t>(block); out[1]=static_cast<std::uint8_t>(block>>8);
    whiten(out,out);  // on-air payload is scrambled; Jitter::push() gets the descrambled block
    return true;
  }
 private:
  const std::uint8_t* data_; std::size_t blocks_; std::uint32_t repeats_, delay_; bool playing_; bool older_=false;
  bool started_=false; std::uint32_t last_=0; std::uint64_t elapsed_=0, start_=0;
  std::atomic<bool> play_req_{false};
 public:
  std::uint32_t plays=0;
};
// Single consumer owns this buffer; SPSC transport is outside it.
class Jitter {
 public:
  static constexpr unsigned kCapacity=32, kPrefill=6, kLateResync=32, kFade=64;
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
      ok=decodeBlock(slot.data.data(),out); slot.valid=false; --queued_; ++decoded;
    }
    if(ok) {
      // After a concealed block, fade the first samples in (no step at the block edge).
      if(concealed_) for(unsigned i=0;i<kFade;i++) out[i]=static_cast<std::int16_t>(out[i]*static_cast<int>(i)/static_cast<int>(kFade));
      concealed_=false; tail_=out[kSamplesPerBlock-1];
    } else {
      // Missing block: ramp the last output sample to zero, then silence. (Repeating the
      // previous block made an audible buzz/click at every loss.)
      ++missing;
      for(unsigned i=0;i<kSamplesPerBlock;i++)
        out[i]= i<kFade ? static_cast<std::int16_t>(tail_*static_cast<int>(kFade-i)/static_cast<int>(kFade)) : 0;
      concealed_=true; tail_=0;
    }
    ++expected_; return ok;
  }
  std::uint32_t duplicates=0, late=0, invalid=0, resets=0, missing=0, decoded=0, startup=0;
 private:
  struct Slot { std::array<std::uint8_t,kBlockBytes> data{}; std::uint16_t seq=0; bool valid=false; };
  void anchor(std::uint16_t seq) {
    for(auto& slot:slots_) slot.valid=false;
    tail_=0; concealed_=true; expected_=seq; queued_=0; anchored_=true; playing_=false; late_run_=0;
  }
  std::array<Slot,kCapacity> slots_{}; std::int16_t tail_=0; bool concealed_=true;
  std::uint16_t expected_=0; unsigned queued_=0, late_run_=0; bool anchored_=false, playing_=false;
};
}
