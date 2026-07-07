#include "pr1_protocol.h"

static void write_be16(uint8_t *dst, uint16_t value)
{
    dst[0] = (uint8_t)(value >> 8);
    dst[1] = (uint8_t)value;
}

static void write_be32(uint8_t *dst, uint32_t value)
{
    dst[0] = (uint8_t)(value >> 24);
    dst[1] = (uint8_t)(value >> 16);
    dst[2] = (uint8_t)(value >> 8);
    dst[3] = (uint8_t)value;
}

static uint16_t read_be16(const uint8_t *src)
{
    return (uint16_t)(((uint16_t)src[0] << 8) | src[1]);
}

static uint32_t read_be32(const uint8_t *src)
{
    return ((uint32_t)src[0] << 24) |
           ((uint32_t)src[1] << 16) |
           ((uint32_t)src[2] << 8) |
           (uint32_t)src[3];
}

void pr1_encode_header(uint8_t *packet,
                       uint8_t flags,
                       uint32_t sequence,
                       uint32_t sample_index)
{
    if (packet == NULL) {
        return;
    }

    packet[0] = PR1_MAGIC_0;
    packet[1] = PR1_MAGIC_1;
    packet[2] = PR1_MAGIC_2;
    packet[3] = PR1_MAGIC_3;
    packet[4] = PR1_PROTOCOL_VERSION;
    packet[5] = flags;
    write_be16(&packet[6], PR1_PAYLOAD_SIZE);
    write_be32(&packet[8], sequence);
    write_be32(&packet[12], sample_index);
}

pr1_packet_status_t pr1_decode_header(const uint8_t *packet,
                                      size_t packet_length,
                                      uint8_t expected_flags,
                                      pr1_packet_header_t *header_out)
{
    if (packet == NULL || header_out == NULL || packet_length != PR1_PACKET_SIZE) {
        return PR1_PACKET_ERR_SIZE;
    }

    if (packet[0] != PR1_MAGIC_0 || packet[1] != PR1_MAGIC_1 ||
        packet[2] != PR1_MAGIC_2 || packet[3] != PR1_MAGIC_3) {
        return PR1_PACKET_ERR_MAGIC;
    }

    if (packet[4] != PR1_PROTOCOL_VERSION) {
        return PR1_PACKET_ERR_VERSION;
    }

    if (packet[5] != expected_flags) {
        return PR1_PACKET_ERR_FLAGS;
    }

    const uint16_t payload_length = read_be16(&packet[6]);
    if (payload_length != PR1_PAYLOAD_SIZE) {
        return PR1_PACKET_ERR_LENGTH;
    }

    header_out->version = packet[4];
    header_out->flags = packet[5];
    header_out->payload_length = payload_length;
    header_out->sequence = read_be32(&packet[8]);
    header_out->sample_index = read_be32(&packet[12]);
    return PR1_PACKET_OK;
}

void pr1_fill_m0_payload(uint8_t *payload, uint32_t sequence)
{
    if (payload == NULL) {
        return;
    }

    for (size_t i = 0; i < PR1_PAYLOAD_SIZE; ++i) {
        payload[i] = (uint8_t)((sequence + (uint32_t)i) & 0xffu);
    }
}

bool pr1_validate_m0_payload(const uint8_t *payload,
                             size_t payload_length,
                             uint32_t sequence,
                             size_t *first_bad_offset)
{
    if (first_bad_offset != NULL) {
        *first_bad_offset = SIZE_MAX;
    }

    if (payload == NULL || payload_length != PR1_PAYLOAD_SIZE) {
        return false;
    }

    for (size_t i = 0; i < PR1_PAYLOAD_SIZE; ++i) {
        const uint8_t expected = (uint8_t)((sequence + (uint32_t)i) & 0xffu);
        if (payload[i] != expected) {
            if (first_bad_offset != NULL) {
                *first_bad_offset = i;
            }
            return false;
        }
    }

    return true;
}

bool pr1_seq_after(uint32_t sequence, uint32_t reference)
{
    const uint32_t distance = sequence - reference;
    return distance != 0u && distance < 0x80000000u;
}

bool pr1_seq_before(uint32_t sequence, uint32_t reference)
{
    return pr1_seq_after(reference, sequence);
}
