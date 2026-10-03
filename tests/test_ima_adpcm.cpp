// PR1 audio prototype: firmware IMA-ADPCM decoder vs host reference (tools/audio/pr1_adpcm.py).
//   test_ima_adpcm <clip.adpcm> <reference_decoded.pcm>   (raw int16 LE written by the python tool)
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <vector>

#include "../firmware/common/pr1_ima_adpcm.hpp"

using namespace pr1::audio;

static std::vector<std::uint8_t> readAll(const char* path) {
  std::vector<std::uint8_t> v;
  FILE* f = std::fopen(path, "rb");
  assert(f != nullptr);
  int c;
  while ((c = std::fgetc(f)) != EOF) v.push_back(static_cast<std::uint8_t>(c));
  std::fclose(f);
  return v;
}

int main(int argc, char** argv) {
  assert(kSamplesPerBlock == 188);
  if (argc < 3) {
    std::printf("usage: test_ima_adpcm clip.adpcm ref.pcm\n");
    return 2;
  }
  const auto blob = readAll(argv[1]);
  const auto ref = readAll(argv[2]);
  assert(blob.size() % kBlockBytes == 0);
  const std::size_t blocks = blob.size() / kBlockBytes;
  assert(ref.size() == blocks * kSamplesPerBlock * 2);
  std::int16_t pcm[kSamplesPerBlock];
  std::size_t mismatches = 0;
  for (std::size_t b = 0; b < blocks; ++b) {
    const bool ok = decodeBlock(&blob[b * kBlockBytes], pcm);
    assert(ok);
    assert(parseHeader(&blob[b * kBlockBytes]).seq == static_cast<std::uint16_t>(b));
    for (std::size_t i = 0; i < kSamplesPerBlock; ++i) {
      const std::size_t o = (b * kSamplesPerBlock + i) * 2;
      const auto r = static_cast<std::int16_t>(static_cast<std::uint16_t>(ref[o] | (ref[o + 1] << 8)));
      if (r != pcm[i]) ++mismatches;
    }
  }
  // Self-contained blocks: decoding block k alone (after skipping k-1) gives the same samples.
  std::int16_t a[kSamplesPerBlock];
  const std::size_t k = blocks / 2;
  decodeBlock(&blob[k * kBlockBytes], a);
  for (std::size_t i = 0; i < kSamplesPerBlock; ++i) {
    const std::size_t o = (k * kSamplesPerBlock + i) * 2;
    assert(a[i] == static_cast<std::int16_t>(static_cast<std::uint16_t>(ref[o] | (ref[o + 1] << 8))));
  }
  // Corrupt header is rejected.
  std::uint8_t bad[kBlockBytes] = {};
  bad[4] = 89;
  assert(!decodeBlock(bad, a));
  std::printf("test_ima_adpcm: %zu blocks, %zu samples, mismatches=%zu -> %s\n", blocks, blocks * kSamplesPerBlock,
              mismatches, mismatches == 0 ? "PASS" : "FAIL");
  return mismatches == 0 ? 0 : 1;
}
