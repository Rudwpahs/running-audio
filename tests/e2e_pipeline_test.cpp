#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <array>
#include <vector>

#include "pr1_protocol.h"

namespace {

constexpr uint32_t kFrameCount = 20U;
constexpr uint32_t kStreamId = 0x55667788U;

void appendLe16(std::vector<uint8_t>* out, uint16_t value) {
  out->push_back(static_cast<uint8_t>(value));
  out->push_back(static_cast<uint8_t>(value >> 8));
}

void appendLe32(std::vector<uint8_t>* out, uint32_t value) {
  out->push_back(static_cast<uint8_t>(value));
  out->push_back(static_cast<uint8_t>(value >> 8));
  out->push_back(static_cast<uint8_t>(value >> 16));
  out->push_back(static_cast<uint8_t>(value >> 24));
}

std::vector<uint8_t> makeWav() {
  std::vector<uint8_t> pcm;
  pcm.reserve(kFrameCount * PR1_PCM_BYTES_PER_FRAME);
  for (uint32_t sample_index = 0;
       sample_index < kFrameCount * PR1_SAMPLES_PER_FRAME;
       ++sample_index) {
    const double phase =
        2.0 * 3.14159265358979323846 * 440.0 * sample_index /
        PR1_SAMPLE_RATE;
    const int16_t sample =
        static_cast<int16_t>(lrint(sin(phase) * 3000.0));
    appendLe16(&pcm, static_cast<uint16_t>(sample));
  }

  std::vector<uint8_t> wav;
  wav.insert(wav.end(), {'R', 'I', 'F', 'F'});
  appendLe32(&wav, 36U + static_cast<uint32_t>(pcm.size()));
  wav.insert(wav.end(), {'W', 'A', 'V', 'E'});
  wav.insert(wav.end(), {'f', 'm', 't', ' '});
  appendLe32(&wav, 16U);
  appendLe16(&wav, 1U);
  appendLe16(&wav, 1U);
  appendLe32(&wav, PR1_SAMPLE_RATE);
  appendLe32(&wav, PR1_SAMPLE_RATE * 2U);
  appendLe16(&wav, 2U);
  appendLe16(&wav, 16U);
  wav.insert(wav.end(), {'d', 'a', 't', 'a'});
  appendLe32(&wav, static_cast<uint32_t>(pcm.size()));
  wav.insert(wav.end(), pcm.begin(), pcm.end());
  return wav;
}

const uint8_t* parseWavPcm(const std::vector<uint8_t>& wav,
                           size_t* pcm_bytes) {
  assert(wav.size() >= 44U);
  assert(memcmp(wav.data(), "RIFF", 4U) == 0);
  assert(memcmp(&wav[8], "WAVE", 4U) == 0);
  assert(memcmp(&wav[12], "fmt ", 4U) == 0);
  assert(pr1_read_le16(&wav[20]) == 1U);
  assert(pr1_read_le16(&wav[22]) == 1U);
  assert(pr1_read_le32(&wav[24]) == PR1_SAMPLE_RATE);
  assert(pr1_read_le16(&wav[34]) == 16U);
  assert(memcmp(&wav[36], "data", 4U) == 0);
  *pcm_bytes = pr1_read_le32(&wav[40]);
  assert(*pcm_bytes + 44U == wav.size());
  return &wav[44];
}

struct ReceivedFrame {
  uint32_t stream_id;
  uint32_t sequence;
  std::array<uint8_t, PR1_PCM_BYTES_PER_FRAME> pcm;
};

}  // namespace

int main() {
  const std::vector<uint8_t> wav = makeWav();
  size_t pcm_bytes = 0U;
  const uint8_t* wav_pcm = parseWavPcm(wav, &pcm_bytes);
  assert(pcm_bytes == kFrameCount * PR1_PCM_BYTES_PER_FRAME);

  Pr1UsbStreamParser usb_parser;
  Pr1FrameReassembler reassembler;
  std::vector<ReceivedFrame> received;
  uint32_t duplicate_count = 0U;
  uint32_t incomplete_count = 0U;
  uint32_t crc_failure_count = 0U;

  for (uint32_t sequence = 0; sequence < kFrameCount; ++sequence) {
    const uint8_t* source =
        &wav_pcm[sequence * PR1_PCM_BYTES_PER_FRAME];

    uint8_t usb_frame[PR1_USB_FRAME_BYTES] = {};
    assert(pr1_encode_usb_frame(
               usb_frame,
               sizeof(usb_frame),
               kStreamId,
               sequence,
               source) ==
           PR1_USB_FRAME_BYTES);

    Pr1UsbFrameView usb_view;
    Pr1UsbParserResult usb_result = Pr1UsbParserResult::None;
    for (uint8_t value : usb_frame) {
      usb_result = usb_parser.push(value, &usb_view);
    }
    assert(usb_result == Pr1UsbParserResult::FrameReady);
    assert(usb_view.stream_id == kStreamId);
    assert(usb_view.sequence == sequence);

    uint8_t fragments[PR1_RF_FRAGMENT_COUNT]
                     [PR1_RF_MAX_PACKET_BYTES] = {};
    size_t lengths[PR1_RF_FRAGMENT_COUNT] = {};
    for (uint8_t index = 0; index < PR1_RF_FRAGMENT_COUNT; ++index) {
      lengths[index] = pr1_encode_rf_fragment(
          fragments[index],
          sizeof(fragments[index]),
          usb_view.stream_id,
          sequence,
          usb_view.frame_crc32,
          usb_view.pcm,
          index);
      assert(lengths[index] <= PR1_RF_MAX_PACKET_BYTES);
    }

    std::vector<uint8_t> order = {0U, 1U, 2U};
    if (sequence == 3U) {
      order = {0U, 1U, 1U, 2U};
    } else if (sequence == 5U) {
      order = {2U, 0U, 1U};
    } else if (sequence == 7U) {
      order = {0U, 2U};
    } else if (sequence == 9U) {
      fragments[1][PR1_RF_HEADER_BYTES + 9U] ^= 0x40U;
    }

    for (uint8_t index : order) {
      Pr1CompletedFrame completed;
      bool dropped_incomplete = false;
      const Pr1ReassemblyResult result = reassembler.push(
          fragments[index],
          lengths[index],
          &completed,
          &dropped_incomplete);
      if (dropped_incomplete) {
        ++incomplete_count;
      }
      if (result == Pr1ReassemblyResult::Duplicate) {
        ++duplicate_count;
      } else if (result == Pr1ReassemblyResult::CrcFailed) {
        ++crc_failure_count;
      } else if (result == Pr1ReassemblyResult::Complete) {
        ReceivedFrame output;
        output.stream_id = completed.stream_id;
        output.sequence = completed.sequence;
        memcpy(output.pcm.data(), completed.pcm, output.pcm.size());
        received.push_back(output);
      } else {
        assert(result == Pr1ReassemblyResult::Accepted);
      }
    }
  }

  assert(duplicate_count == 1U);
  assert(incomplete_count == 1U);
  assert(crc_failure_count == 1U);
  assert(received.size() == kFrameCount - 2U);

  for (const ReceivedFrame& frame : received) {
    assert(frame.stream_id == kStreamId);
    assert(frame.sequence != 7U);
    assert(frame.sequence != 9U);
    const uint8_t* source =
        &wav_pcm[frame.sequence * PR1_PCM_BYTES_PER_FRAME];
    assert(memcmp(frame.pcm.data(), source, frame.pcm.size()) == 0);
  }

  printf(
      "PASS wav_frames=%u restored=%zu dropped=2 duplicate=%u "
      "incomplete=%u crc_fail=%u max_rf_bytes=%u\n",
      static_cast<unsigned>(kFrameCount),
      received.size(),
      static_cast<unsigned>(duplicate_count),
      static_cast<unsigned>(incomplete_count),
      static_cast<unsigned>(crc_failure_count),
      static_cast<unsigned>(PR1_RF_MAX_PACKET_BYTES));
  return 0;
}
