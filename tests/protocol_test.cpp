#include <assert.h>
#include <stdint.h>
#include <string.h>

#include "pr1_protocol.h"

namespace {

constexpr uint32_t kTestStreamId = 0x10203040U;

void fillPcm(uint8_t* pcm, uint8_t seed) {
  for (size_t i = 0; i < PR1_PCM_BYTES_PER_FRAME; ++i) {
    pcm[i] = static_cast<uint8_t>((i * 37U + seed) & 0xFFU);
  }
}

void encodeFragments(const uint8_t* pcm,
                     uint32_t sequence,
                     uint8_t packets[PR1_RF_FRAGMENT_COUNT]
                                    [PR1_RF_MAX_PACKET_BYTES],
                     size_t lengths[PR1_RF_FRAGMENT_COUNT]) {
  const uint32_t crc = pr1_crc32(pcm, PR1_PCM_BYTES_PER_FRAME);
  for (uint8_t index = 0; index < PR1_RF_FRAGMENT_COUNT; ++index) {
    lengths[index] = pr1_encode_rf_fragment(
        packets[index],
        PR1_RF_MAX_PACKET_BYTES,
        kTestStreamId,
        sequence,
        crc,
        pcm,
        index);
    assert(lengths[index] != 0U);
    assert(lengths[index] <= PR1_RF_MAX_PACKET_BYTES);
  }
}

void testCrcAndFragmentCodec() {
  static const uint8_t known[] = "123456789";
  assert(pr1_crc32(known, sizeof(known) - 1U) == 0xCBF43926U);

  uint8_t pcm[PR1_PCM_BYTES_PER_FRAME];
  fillPcm(pcm, 7U);
  uint8_t packets[PR1_RF_FRAGMENT_COUNT][PR1_RF_MAX_PACKET_BYTES] = {};
  size_t lengths[PR1_RF_FRAGMENT_COUNT] = {};
  encodeFragments(pcm, 0x12345678U, packets, lengths);

  assert(lengths[0] == 127U);
  assert(lengths[1] == 127U);
  assert(lengths[2] == 123U);
  assert(pr1_fragment_payload_bytes(0U) == 108U);
  assert(pr1_fragment_payload_bytes(1U) == 108U);
  assert(pr1_fragment_payload_bytes(2U) == 104U);
  assert(pr1_fragment_payload_bytes(3U) == 0U);

  for (uint8_t index = 0; index < PR1_RF_FRAGMENT_COUNT; ++index) {
    Pr1DecodedFragment decoded;
    assert(pr1_decode_rf_fragment(packets[index], lengths[index], &decoded));
    assert(decoded.stream_id == kTestStreamId);
    assert(decoded.sequence == 0x12345678U);
    assert(decoded.fragment_index == index);
    assert(decoded.fragment_count == PR1_RF_FRAGMENT_COUNT);
    assert(decoded.payload_bytes == pr1_fragment_payload_bytes(index));
    assert(decoded.frame_crc32 == pr1_crc32(pcm, sizeof(pcm)));
  }

  packets[0][0] ^= 1U;
  Pr1DecodedFragment decoded;
  assert(!pr1_decode_rf_fragment(packets[0], lengths[0], &decoded));
  packets[0][0] ^= 1U;
  assert(!pr1_decode_rf_fragment(packets[0], lengths[0] - 1U, &decoded));
  packets[0][13] = 2U;
  assert(!pr1_decode_rf_fragment(packets[0], lengths[0], &decoded));
}

void testReassemblyAndFaults() {
  uint8_t pcm[PR1_PCM_BYTES_PER_FRAME];
  fillPcm(pcm, 19U);
  uint8_t packets[PR1_RF_FRAGMENT_COUNT][PR1_RF_MAX_PACKET_BYTES] = {};
  size_t lengths[PR1_RF_FRAGMENT_COUNT] = {};
  encodeFragments(pcm, 42U, packets, lengths);

  Pr1FrameReassembler reassembler;
  Pr1CompletedFrame completed;
  bool dropped = false;

  assert(reassembler.push(packets[1], lengths[1], &completed, &dropped) ==
         Pr1ReassemblyResult::Accepted);
  assert(!dropped);
  assert(reassembler.push(packets[1], lengths[1], &completed, &dropped) ==
         Pr1ReassemblyResult::Duplicate);
  assert(reassembler.push(packets[0], lengths[0], &completed, &dropped) ==
         Pr1ReassemblyResult::Accepted);
  assert(reassembler.push(packets[2], lengths[2], &completed, &dropped) ==
         Pr1ReassemblyResult::Complete);
  assert(completed.sequence == 42U);
  assert(memcmp(completed.pcm, pcm, sizeof(pcm)) == 0);
  assert(reassembler.push(packets[0], lengths[0], &completed, &dropped) ==
         Pr1ReassemblyResult::Stale);

  uint8_t next_pcm[PR1_PCM_BYTES_PER_FRAME];
  fillPcm(next_pcm, 20U);
  uint8_t next_packets[PR1_RF_FRAGMENT_COUNT][PR1_RF_MAX_PACKET_BYTES] = {};
  size_t next_lengths[PR1_RF_FRAGMENT_COUNT] = {};
  encodeFragments(next_pcm, 44U, next_packets, next_lengths);

  reassembler.resetAll();
  assert(reassembler.push(packets[0], lengths[0], &completed, &dropped) ==
         Pr1ReassemblyResult::Accepted);
  assert(reassembler.push(next_packets[0],
                          next_lengths[0],
                          &completed,
                          &dropped) == Pr1ReassemblyResult::Accepted);
  assert(dropped);

  reassembler.resetAll();
  encodeFragments(pcm, 50U, packets, lengths);
  packets[1][PR1_RF_HEADER_BYTES + 4U] ^= 0x80U;
  assert(reassembler.push(packets[0], lengths[0], &completed) ==
         Pr1ReassemblyResult::Accepted);
  assert(reassembler.push(packets[1], lengths[1], &completed) ==
         Pr1ReassemblyResult::Accepted);
  assert(reassembler.push(packets[2], lengths[2], &completed) ==
         Pr1ReassemblyResult::CrcFailed);
}

void testSequenceWrap() {
  assert(pr1_sequence_after(0U, 0xFFFFFFFFU));
  assert(pr1_sequence_after(1U, 0xFFFFFFFFU));
  assert(!pr1_sequence_after(0xFFFFFFFFU, 0U));
  assert(!pr1_sequence_after(7U, 7U));

  uint8_t pcm[PR1_PCM_BYTES_PER_FRAME];
  fillPcm(pcm, 2U);
  uint8_t packets[PR1_RF_FRAGMENT_COUNT][PR1_RF_MAX_PACKET_BYTES] = {};
  size_t lengths[PR1_RF_FRAGMENT_COUNT] = {};
  Pr1FrameReassembler reassembler;
  Pr1CompletedFrame completed;

  encodeFragments(pcm, 0xFFFFFFFFU, packets, lengths);
  for (uint8_t i = 0; i < PR1_RF_FRAGMENT_COUNT; ++i) {
    const Pr1ReassemblyResult result =
        reassembler.push(packets[i], lengths[i], &completed);
    assert(result == (i == PR1_RF_FRAGMENT_COUNT - 1U
                          ? Pr1ReassemblyResult::Complete
                          : Pr1ReassemblyResult::Accepted));
  }

  encodeFragments(pcm, 0U, packets, lengths);
  for (uint8_t i = 0; i < PR1_RF_FRAGMENT_COUNT; ++i) {
    const Pr1ReassemblyResult result =
        reassembler.push(packets[i], lengths[i], &completed);
    assert(result == (i == PR1_RF_FRAGMENT_COUNT - 1U
                          ? Pr1ReassemblyResult::Complete
                          : Pr1ReassemblyResult::Accepted));
  }
  assert(completed.sequence == 0U);
}

void testStreamRestart() {
  uint8_t pcm[PR1_PCM_BYTES_PER_FRAME];
  fillPcm(pcm, 33U);
  uint8_t packets[PR1_RF_FRAGMENT_COUNT][PR1_RF_MAX_PACKET_BYTES] = {};
  size_t lengths[PR1_RF_FRAGMENT_COUNT] = {};
  Pr1FrameReassembler reassembler;
  Pr1CompletedFrame completed;

  const uint32_t old_crc = pr1_crc32(pcm, sizeof(pcm));
  for (uint8_t index = 0; index < PR1_RF_FRAGMENT_COUNT; ++index) {
    lengths[index] = pr1_encode_rf_fragment(
        packets[index],
        sizeof(packets[index]),
        0x11111111U,
        9000U,
        old_crc,
        pcm,
        index);
    assert(lengths[index] != 0U);
    const Pr1ReassemblyResult result =
        reassembler.push(packets[index], lengths[index], &completed);
    assert(result == (index == PR1_RF_FRAGMENT_COUNT - 1U
                          ? Pr1ReassemblyResult::Complete
                          : Pr1ReassemblyResult::Accepted));
  }

  bool stream_changed = false;
  const uint32_t new_crc = pr1_crc32(pcm, sizeof(pcm));
  for (uint8_t index = 0; index < PR1_RF_FRAGMENT_COUNT; ++index) {
    lengths[index] = pr1_encode_rf_fragment(
        packets[index],
        sizeof(packets[index]),
        0x22222222U,
        0U,
        new_crc,
        pcm,
        index);
    assert(lengths[index] != 0U);
    const Pr1ReassemblyResult result = reassembler.push(
        packets[index],
        lengths[index],
        &completed,
        nullptr,
        &stream_changed);
    if (index == 0U) {
      assert(stream_changed);
    } else {
      assert(!stream_changed);
    }
    assert(result == (index == PR1_RF_FRAGMENT_COUNT - 1U
                          ? Pr1ReassemblyResult::Complete
                          : Pr1ReassemblyResult::Accepted));
  }
  assert(completed.stream_id == 0x22222222U);
  assert(completed.sequence == 0U);

  lengths[0] = pr1_encode_rf_fragment(
      packets[0],
      sizeof(packets[0]),
      0x11111111U,
      9001U,
      old_crc,
      pcm,
      0U);
  assert(reassembler.push(packets[0], lengths[0], &completed) ==
         Pr1ReassemblyResult::Stale);
}

void testUsbStreamParser() {
  uint8_t pcm[PR1_PCM_BYTES_PER_FRAME];
  for (size_t i = 0; i < sizeof(pcm); ++i) {
    pcm[i] = static_cast<uint8_t>((i * 17U + 3U) & 0xFFU);
  }
  assert(pr1_crc32(pcm, sizeof(pcm)) == 0xDB4A46ECU);
  uint8_t usb_frame[PR1_USB_FRAME_BYTES] = {};
  assert(pr1_encode_usb_frame(
             usb_frame,
             sizeof(usb_frame),
             kTestStreamId,
             0x89ABCDEFU,
             pcm) ==
         PR1_USB_FRAME_BYTES);
  static const uint8_t expected_header[PR1_USB_HEADER_BYTES] = {
      0x50, 0x52, 0x31, 0x55, 0x03, 0x01, 0x40, 0x01,
      0x40, 0x30, 0x20, 0x10, 0xEF, 0xCD, 0xAB, 0x89,
      0xEC, 0x46, 0x4A, 0xDB};
  assert(memcmp(usb_frame, expected_header, sizeof(expected_header)) == 0);

  Pr1UsbStreamParser parser;
  Pr1UsbFrameView view;
  const uint8_t noise[] = {'x', 'P', 'x', 'P', 'R', '1', 'x'};
  for (uint8_t value : noise) {
    assert(parser.push(value, &view) == Pr1UsbParserResult::None);
  }

  Pr1UsbParserResult result = Pr1UsbParserResult::None;
  for (uint8_t value : usb_frame) {
    result = parser.push(value, &view);
  }
  assert(result == Pr1UsbParserResult::FrameReady);
  assert(view.stream_id == kTestStreamId);
  assert(view.sequence == 0x89ABCDEFU);
  assert(view.frame_crc32 == pr1_crc32(pcm, sizeof(pcm)));
  assert(memcmp(view.pcm, pcm, sizeof(pcm)) == 0);

  usb_frame[PR1_USB_HEADER_BYTES + 17U] ^= 0x01U;
  result = Pr1UsbParserResult::None;
  for (uint8_t value : usb_frame) {
    const Pr1UsbParserResult current = parser.push(value, &view);
    if (current != Pr1UsbParserResult::None) {
      result = current;
    }
  }
  assert(result == Pr1UsbParserResult::CrcRejected);

  assert(pr1_encode_usb_frame(
             usb_frame, sizeof(usb_frame), kTestStreamId, 4U, pcm) ==
         PR1_USB_FRAME_BYTES);
  usb_frame[4] = 99U;
  result = Pr1UsbParserResult::None;
  for (size_t i = 0; i < PR1_USB_HEADER_BYTES; ++i) {
    result = parser.push(usb_frame[i], &view);
  }
  assert(result == Pr1UsbParserResult::HeaderRejected);
}

}  // namespace

int main() {
  testCrcAndFragmentCodec();
  testReassemblyAndFaults();
  testSequenceWrap();
  testStreamRestart();
  testUsbStreamParser();
  return 0;
}
