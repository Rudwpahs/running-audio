#include <assert.h>
#include <stdint.h>
#include <string.h>

#include "pr1_protocol.h"

int main() {
  int16_t samples[PR1_SAMPLES_PER_PACKET];
  for (size_t i = 0; i < PR1_SAMPLES_PER_PACKET; ++i) {
    samples[i] = static_cast<int16_t>(i - 60);
  }

  uint8_t packet[PR1_RF_PACKET_BYTES] = {};
  pr1_encode_rf_packet(packet, 0xFFFFFFFFU, samples);

  Pr1DecodedPacket decoded;
  assert(pr1_decode_rf_packet(packet, sizeof(packet), &decoded));
  assert(decoded.sequence == 0xFFFFFFFFU);
  assert(decoded.sample_index ==
         0xFFFFFFFFU * PR1_SAMPLES_PER_PACKET);
  assert(memcmp(decoded.samples, samples, sizeof(samples)) == 0);

  packet[0] ^= 1U;
  assert(!pr1_decode_rf_packet(packet, sizeof(packet), &decoded));
  assert(pr1_sequence_after(0U, 0xFFFFFFFFU));
  assert(!pr1_sequence_after(7U, 7U));
  return 0;
}
