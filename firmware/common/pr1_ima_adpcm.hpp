#pragma once

// PR1 IMA-ADPCM block decoder (standard IMA-ADPCM). The August Pr1ImaAdpcm.h source was never
// committed; this re-implementation is bit-exact with the host reference tools/audio/pr1_adpcm.py
// (checked by tests/test_ima_adpcm.cpp).
//
// Block = one 100-byte RF payload: u16 block_seq, i16 predictor, u8 step_index, u8 flags,
// then 94 bytes = 188 4-bit codes (low nibble first). Self-contained: the decoder state comes
// from the header, so a lost block never desynchronises the stream.

#include <cstddef>
#include <cstdint>

#if defined(ARDUINO_ARCH_ESP32) || defined(ESP_PLATFORM)
#include "esp_attr.h"
#define PR1_AUDIO_IRAM IRAM_ATTR
#define PR1_AUDIO_DRAM DRAM_ATTR
#else
#define PR1_AUDIO_IRAM
#define PR1_AUDIO_DRAM
#endif

namespace pr1::audio {

constexpr std::size_t kBlockBytes = 100;
constexpr std::size_t kBlockHeaderBytes = 6;
constexpr std::size_t kSamplesPerBlock = (kBlockBytes - kBlockHeaderBytes) * 2;  // 188
constexpr std::uint32_t kSampleRate = 32000;
constexpr std::uint8_t kFlagLast = 0x01;

struct BlockHeader {
  std::uint16_t seq = 0;
  std::int16_t predictor = 0;
  std::uint8_t step_index = 0;
  std::uint8_t flags = 0;
};

PR1_AUDIO_DRAM static const std::int16_t kStep[89] = {
    7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 19, 21, 23, 25, 28, 31, 34, 37, 41, 45, 50, 55, 60, 66, 73, 80, 88,
    97, 107, 118, 130, 143, 157, 173, 190, 209, 230, 253, 279, 307, 337, 371, 408, 449, 494, 544, 598, 658,
    724, 796, 876, 963, 1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066, 2272, 2499, 2749, 3024, 3327, 3660,
    4026, 4428, 4871, 5358, 5894, 6484, 7132, 7845, 8630, 9493, 10442, 11487, 12635, 13899, 15289, 16818,
    18500, 20350, 22385, 24623, 27086, 29794, 32767};
PR1_AUDIO_DRAM static const std::int8_t kIndex[8] = {-1, -1, -1, -1, 2, 4, 6, 8};

inline BlockHeader parseHeader(const std::uint8_t* b) {
  BlockHeader h;
  h.seq = static_cast<std::uint16_t>(b[0] | (b[1] << 8));
  h.predictor = static_cast<std::int16_t>(static_cast<std::uint16_t>(b[2] | (b[3] << 8)));
  h.step_index = b[4];
  h.flags = b[5];
  return h;
}

// Decodes one block into 188 samples. Returns false (and writes nothing) on a corrupt header.
PR1_AUDIO_IRAM inline bool decodeBlock(const std::uint8_t* block, std::int16_t* out) {
  const BlockHeader h = parseHeader(block);
  if (h.step_index > 88) return false;
  std::int32_t pred = h.predictor;
  std::int32_t idx = h.step_index;
  std::size_t n = 0;
  for (std::size_t i = kBlockHeaderBytes; i < kBlockBytes; ++i) {
    const std::uint8_t byte = block[i];
    for (int half = 0; half < 2; ++half) {
      const std::uint8_t code = half == 0 ? (byte & 0x0F) : (byte >> 4);
      const std::int32_t step = kStep[idx];
      std::int32_t diff = step >> 3;
      if (code & 4) diff += step;
      if (code & 2) diff += step >> 1;
      if (code & 1) diff += step >> 2;
      pred = (code & 8) ? pred - diff : pred + diff;
      if (pred > 32767) pred = 32767;
      if (pred < -32768) pred = -32768;
      idx += kIndex[code & 7];
      if (idx < 0) idx = 0;
      if (idx > 88) idx = 88;
      out[n++] = static_cast<std::int16_t>(pred);
    }
  }
  return true;
}

}  // namespace pr1::audio
