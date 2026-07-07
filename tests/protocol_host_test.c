#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "pr1_protocol.h"

int main(void)
{
    uint8_t packet[PR1_PACKET_SIZE] = {0};
    pr1_encode_header(packet, PR1_FLAG_M0, 0x12345678u, 0x89abcdefu);
    pr1_fill_m0_payload(&packet[PR1_HEADER_SIZE], 0x12345678u);

    pr1_packet_header_t header;
    assert(pr1_decode_header(packet, sizeof(packet), PR1_FLAG_M0, &header) == PR1_PACKET_OK);
    assert(header.sequence == 0x12345678u);
    assert(header.sample_index == 0x89abcdefu);
    assert(header.payload_length == PR1_PAYLOAD_SIZE);
    assert(pr1_validate_m0_payload(&packet[PR1_HEADER_SIZE], PR1_PAYLOAD_SIZE,
                                   header.sequence, NULL));

    packet[PR1_HEADER_SIZE + 10] ^= 1u;
    size_t bad_offset = SIZE_MAX;
    assert(!pr1_validate_m0_payload(&packet[PR1_HEADER_SIZE], PR1_PAYLOAD_SIZE,
                                    header.sequence, &bad_offset));
    assert(bad_offset == 10u);

    assert(pr1_seq_after(0u, UINT32_MAX));
    assert(pr1_seq_before(UINT32_MAX, 0u));
    assert(!pr1_seq_after(7u, 7u));

    puts("protocol tests passed");
    return 0;
}
