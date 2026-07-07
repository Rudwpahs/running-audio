#pragma once

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "pr1_config.h"

static constexpr uint8_t PR1_RF_MAGIC_0 = 'P';
static constexpr uint8_t PR1_RF_MAGIC_1 = '2';
static constexpr uint8_t PR1_RF_VERSION = 0x02;
static constexpr uint8_t PR1_RF_FLAG_AUDIO = 0x01;

struct Pr1DecodedPacket {
  uint32_t sequence;
  uint32_t sample_index;
  const int16_t* samples;
};

inline void pr1_write_be32(uint8_t* dst, uint32_t value) {
  dst[0] = static_cast<uint8_t>(value >> 24);
  dst[1] = static_cast<uint8_t>(value >> 16);
  dst[2] = static_cast<uint8_t>(value >> 8);
  dst[3] = static_cast<uint8_t>(value);
}

inline uint32_t pr1_read_be32(const uint8_t* src) {
  return (static_cast<uint32_t>(src[0]) << 24) |
         (static_cast<uint32_t>(src[1]) << 16) |
         (static_cast<uint32_t>(src[2]) << 8) |
         static_cast<uint32_t>(src[3]);
}

inline uint32_t pr1_read_le32(const uint8_t* src) {
  return static_cast<uint32_t>(src[0]) |
         (static_cast<uint32_t>(src[1]) << 8) |
         (static_cast<uint32_t>(src[2]) << 16) |
         (static_cast<uint32_t>(src[3]) << 24);
}

inline void pr1_encode_rf_packet(uint8_t* packet,
                                 uint32_t sequence,
                                 const int16_t* samples) {
  packet[0] = PR1_RF_MAGIC_0;
  packet[1] = PR1_RF_MAGIC_1;
  packet[2] = PR1_RF_VERSION;
  packet[3] = PR1_RF_FLAG_AUDIO;
  pr1_write_be32(&packet[4], sequence);
  pr1_write_be32(&packet[8], sequence * PR1_SAMPLES_PER_PACKET);
  memcpy(&packet[PR1_RF_HEADER_BYTES], samples, PR1_PCM_BYTES_PER_PACKET);
}

inline bool pr1_decode_rf_packet(const uint8_t* packet,
                                 size_t length,
                                 Pr1DecodedPacket* decoded) {
  if (packet == nullptr || decoded == nullptr ||
      length != PR1_RF_PACKET_BYTES) {
    return false;
  }
  if (packet[0] != PR1_RF_MAGIC_0 || packet[1] != PR1_RF_MAGIC_1 ||
      packet[2] != PR1_RF_VERSION || packet[3] != PR1_RF_FLAG_AUDIO) {
    return false;
  }

  decoded->sequence = pr1_read_be32(&packet[4]);
  decoded->sample_index = pr1_read_be32(&packet[8]);
  if (decoded->sample_index != decoded->sequence * PR1_SAMPLES_PER_PACKET) {
    return false;
  }
  decoded->samples =
      reinterpret_cast<const int16_t*>(&packet[PR1_RF_HEADER_BYTES]);
  return true;
}

inline bool pr1_sequence_after(uint32_t sequence, uint32_t reference) {
  const uint32_t distance = sequence - reference;
  return distance != 0U && distance < 0x80000000U;
}
