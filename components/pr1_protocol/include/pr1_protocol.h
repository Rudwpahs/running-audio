#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PR1_PROTOCOL_VERSION        0x01u
#define PR1_FLAG_M0                 0x01u
#define PR1_FLAG_M1                 0x02u
#define PR1_HEADER_SIZE             16u
#define PR1_PAYLOAD_SIZE            640u
#define PR1_PACKET_SIZE             (PR1_HEADER_SIZE + PR1_PAYLOAD_SIZE)
#define PR1_SAMPLES_PER_PACKET      320u
#define PR1_PACKET_INTERVAL_US      10000LL
#define PR1_UDP_PORT                40100u

#define PR1_WIFI_SSID               "PR1_AUDIO_LINK"
#define PR1_WIFI_PASSWORD           "PR1audio2026"
#define PR1_WIFI_CHANNEL            6u
#define PR1_TX_IPV4                 "192.168.4.1"
#define PR1_RX_IPV4                 "192.168.4.2"
#define PR1_NETMASK_IPV4            "255.255.255.0"

#define PR1_MAGIC_0                 ((uint8_t)'P')
#define PR1_MAGIC_1                 ((uint8_t)'R')
#define PR1_MAGIC_2                 ((uint8_t)'1')
#define PR1_MAGIC_3                 ((uint8_t)'A')

typedef struct {
    uint8_t version;
    uint8_t flags;
    uint16_t payload_length;
    uint32_t sequence;
    uint32_t sample_index;
} pr1_packet_header_t;

typedef enum {
    PR1_PACKET_OK = 0,
    PR1_PACKET_ERR_SIZE,
    PR1_PACKET_ERR_MAGIC,
    PR1_PACKET_ERR_VERSION,
    PR1_PACKET_ERR_FLAGS,
    PR1_PACKET_ERR_LENGTH,
} pr1_packet_status_t;

void pr1_encode_header(uint8_t *packet,
                       uint8_t flags,
                       uint32_t sequence,
                       uint32_t sample_index);

pr1_packet_status_t pr1_decode_header(const uint8_t *packet,
                                      size_t packet_length,
                                      uint8_t expected_flags,
                                      pr1_packet_header_t *header_out);

void pr1_fill_m0_payload(uint8_t *payload, uint32_t sequence);

bool pr1_validate_m0_payload(const uint8_t *payload,
                             size_t payload_length,
                             uint32_t sequence,
                             size_t *first_bad_offset);

bool pr1_seq_after(uint32_t sequence, uint32_t reference);
bool pr1_seq_before(uint32_t sequence, uint32_t reference);

#ifdef __cplusplus
}
#endif
