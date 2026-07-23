#pragma once

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "pr1_config.h"

static constexpr uint8_t PR1_RF_MAGIC_0 = 'P';
static constexpr uint8_t PR1_RF_MAGIC_1 = '3';
static constexpr uint8_t PR1_PROTOCOL_VERSION = 0x03;
static constexpr uint8_t PR1_RF_FLAG_AUDIO = 0x01;

enum class Pr1ReassemblyResult : uint8_t {
  Rejected,
  Accepted,
  Duplicate,
  Complete,
  CrcFailed,
  Stale,
};

enum class Pr1UsbParserResult : uint8_t {
  None,
  FrameReady,
  HeaderRejected,
  CrcRejected,
};

struct Pr1DecodedFragment {
  uint32_t stream_id;
  uint32_t sequence;
  uint32_t frame_crc32;
  uint8_t fragment_index;
  uint8_t fragment_count;
  uint8_t payload_bytes;
  const uint8_t* payload;
};

struct Pr1CompletedFrame {
  uint32_t stream_id;
  uint32_t sequence;
  uint8_t pcm[PR1_PCM_BYTES_PER_FRAME];
};

struct Pr1UsbFrameView {
  uint32_t stream_id;
  uint32_t sequence;
  uint32_t frame_crc32;
  const uint8_t* pcm;
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

inline void pr1_write_le16(uint8_t* dst, uint16_t value) {
  dst[0] = static_cast<uint8_t>(value);
  dst[1] = static_cast<uint8_t>(value >> 8);
}

inline uint16_t pr1_read_le16(const uint8_t* src) {
  return static_cast<uint16_t>(src[0]) |
         static_cast<uint16_t>(static_cast<uint16_t>(src[1]) << 8);
}

inline void pr1_write_le32(uint8_t* dst, uint32_t value) {
  dst[0] = static_cast<uint8_t>(value);
  dst[1] = static_cast<uint8_t>(value >> 8);
  dst[2] = static_cast<uint8_t>(value >> 16);
  dst[3] = static_cast<uint8_t>(value >> 24);
}

inline uint32_t pr1_read_le32(const uint8_t* src) {
  return static_cast<uint32_t>(src[0]) |
         (static_cast<uint32_t>(src[1]) << 8) |
         (static_cast<uint32_t>(src[2]) << 16) |
         (static_cast<uint32_t>(src[3]) << 24);
}

inline uint32_t pr1_crc32(const uint8_t* data, size_t length) {
  if (data == nullptr && length != 0U) {
    return 0U;
  }

  uint32_t crc = 0xFFFFFFFFU;
  for (size_t i = 0; i < length; ++i) {
    crc ^= data[i];
    for (uint8_t bit = 0; bit < 8U; ++bit) {
      const uint32_t mask =
          static_cast<uint32_t>(-static_cast<int32_t>(crc & 1U));
      crc = (crc >> 1U) ^ (0xEDB88320U & mask);
    }
  }
  return ~crc;
}

inline size_t pr1_fragment_payload_bytes(uint8_t fragment_index) {
  if (fragment_index >= PR1_RF_FRAGMENT_COUNT) {
    return 0U;
  }
  const size_t offset =
      static_cast<size_t>(fragment_index) * PR1_RF_FRAGMENT_DATA_BYTES;
  const size_t remaining = PR1_PCM_BYTES_PER_FRAME - offset;
  return (remaining < PR1_RF_FRAGMENT_DATA_BYTES)
             ? remaining
             : PR1_RF_FRAGMENT_DATA_BYTES;
}

inline size_t pr1_encode_rf_fragment(uint8_t* packet,
                                     size_t capacity,
                                     uint32_t stream_id,
                                     uint32_t sequence,
                                     uint32_t frame_crc32,
                                     const uint8_t* pcm,
                                     uint8_t fragment_index) {
  const size_t payload_bytes =
      pr1_fragment_payload_bytes(fragment_index);
  if (packet == nullptr || pcm == nullptr || payload_bytes == 0U ||
      capacity < PR1_RF_HEADER_BYTES + payload_bytes) {
    return 0U;
  }

  packet[0] = PR1_RF_MAGIC_0;
  packet[1] = PR1_RF_MAGIC_1;
  packet[2] = PR1_PROTOCOL_VERSION;
  packet[3] = PR1_RF_FLAG_AUDIO;
  pr1_write_be32(&packet[4], stream_id);
  pr1_write_be32(&packet[8], sequence);
  packet[12] = fragment_index;
  packet[13] = PR1_RF_FRAGMENT_COUNT;
  packet[14] = static_cast<uint8_t>(payload_bytes);
  pr1_write_be32(&packet[15], frame_crc32);

  const size_t source_offset =
      static_cast<size_t>(fragment_index) * PR1_RF_FRAGMENT_DATA_BYTES;
  memcpy(&packet[PR1_RF_HEADER_BYTES],
         &pcm[source_offset],
         payload_bytes);
  return PR1_RF_HEADER_BYTES + payload_bytes;
}

inline bool pr1_decode_rf_fragment(const uint8_t* packet,
                                   size_t length,
                                   Pr1DecodedFragment* decoded) {
  if (packet == nullptr || decoded == nullptr ||
      length < PR1_RF_HEADER_BYTES || length > PR1_RF_MAX_PACKET_BYTES) {
    return false;
  }
  if (packet[0] != PR1_RF_MAGIC_0 || packet[1] != PR1_RF_MAGIC_1 ||
      packet[2] != PR1_PROTOCOL_VERSION ||
      packet[3] != PR1_RF_FLAG_AUDIO) {
    return false;
  }

  const uint8_t fragment_index = packet[12];
  const uint8_t fragment_count = packet[13];
  const uint8_t payload_bytes = packet[14];
  const size_t expected_payload =
      pr1_fragment_payload_bytes(fragment_index);
  if (fragment_count != PR1_RF_FRAGMENT_COUNT ||
      expected_payload == 0U || payload_bytes != expected_payload ||
      length != PR1_RF_HEADER_BYTES + expected_payload) {
    return false;
  }

  decoded->stream_id = pr1_read_be32(&packet[4]);
  decoded->sequence = pr1_read_be32(&packet[8]);
  decoded->fragment_index = fragment_index;
  decoded->fragment_count = fragment_count;
  decoded->payload_bytes = payload_bytes;
  decoded->frame_crc32 = pr1_read_be32(&packet[15]);
  decoded->payload = &packet[PR1_RF_HEADER_BYTES];
  return true;
}

inline bool pr1_sequence_after(uint32_t sequence, uint32_t reference) {
  const uint32_t distance = sequence - reference;
  return distance != 0U && distance < 0x80000000U;
}

inline size_t pr1_encode_usb_frame(uint8_t* frame,
                                   size_t capacity,
                                   uint32_t stream_id,
                                   uint32_t sequence,
                                   const uint8_t* pcm) {
  if (frame == nullptr || pcm == nullptr ||
      capacity < PR1_USB_FRAME_BYTES) {
    return 0U;
  }

  memcpy(frame, PR1_USB_MAGIC, sizeof(PR1_USB_MAGIC));
  frame[4] = PR1_PROTOCOL_VERSION;
  frame[5] = PR1_RF_FLAG_AUDIO;
  pr1_write_le16(&frame[6], PR1_PCM_BYTES_PER_FRAME);
  pr1_write_le32(&frame[8], stream_id);
  pr1_write_le32(&frame[12], sequence);
  const uint32_t frame_crc32 = pr1_crc32(pcm, PR1_PCM_BYTES_PER_FRAME);
  pr1_write_le32(&frame[16], frame_crc32);
  memcpy(&frame[PR1_USB_HEADER_BYTES], pcm, PR1_PCM_BYTES_PER_FRAME);
  return PR1_USB_FRAME_BYTES;
}

class Pr1UsbStreamParser {
 public:
  Pr1UsbStreamParser() {
    reset();
  }

  Pr1UsbParserResult push(uint8_t value, Pr1UsbFrameView* frame) {
    if (frame != nullptr) {
      frame->stream_id = 0U;
      frame->sequence = 0U;
      frame->frame_crc32 = 0U;
      frame->pcm = nullptr;
    }

    if (state_ == State::FindMagic) {
      if (value == PR1_USB_MAGIC[magic_position_]) {
        ++magic_position_;
        if (magic_position_ == sizeof(PR1_USB_MAGIC)) {
          memcpy(header_, PR1_USB_MAGIC, sizeof(PR1_USB_MAGIC));
          header_position_ = sizeof(PR1_USB_MAGIC);
          magic_position_ = 0U;
          state_ = State::ReadHeader;
        }
      } else {
        magic_position_ = (value == PR1_USB_MAGIC[0]) ? 1U : 0U;
      }
      return Pr1UsbParserResult::None;
    }

    if (state_ == State::ReadHeader) {
      header_[header_position_++] = value;
      if (header_position_ != PR1_USB_HEADER_BYTES) {
        return Pr1UsbParserResult::None;
      }

      if (header_[4] != PR1_PROTOCOL_VERSION ||
          header_[5] != PR1_RF_FLAG_AUDIO ||
          pr1_read_le16(&header_[6]) != PR1_PCM_BYTES_PER_FRAME) {
        reset();
        return Pr1UsbParserResult::HeaderRejected;
      }

      stream_id_ = pr1_read_le32(&header_[8]);
      sequence_ = pr1_read_le32(&header_[12]);
      expected_crc32_ = pr1_read_le32(&header_[16]);
      pcm_position_ = 0U;
      state_ = State::ReadPcm;
      return Pr1UsbParserResult::None;
    }

    pcm_[pcm_position_++] = value;
    if (pcm_position_ != PR1_PCM_BYTES_PER_FRAME) {
      return Pr1UsbParserResult::None;
    }

    const uint32_t actual_crc32 =
        pr1_crc32(pcm_, PR1_PCM_BYTES_PER_FRAME);
    if (actual_crc32 != expected_crc32_) {
      reset();
      return Pr1UsbParserResult::CrcRejected;
    }

    if (frame != nullptr) {
      frame->stream_id = stream_id_;
      frame->sequence = sequence_;
      frame->frame_crc32 = expected_crc32_;
      frame->pcm = pcm_;
    }
    state_ = State::FindMagic;
    magic_position_ = 0U;
    header_position_ = 0U;
    pcm_position_ = 0U;
    return Pr1UsbParserResult::FrameReady;
  }

  void reset() {
    state_ = State::FindMagic;
    magic_position_ = 0U;
    header_position_ = 0U;
    pcm_position_ = 0U;
    stream_id_ = 0U;
    sequence_ = 0U;
    expected_crc32_ = 0U;
    memset(header_, 0, sizeof(header_));
    memset(pcm_, 0, sizeof(pcm_));
  }

 private:
  enum class State : uint8_t {
    FindMagic,
    ReadHeader,
    ReadPcm,
  };

  State state_ = State::FindMagic;
  size_t magic_position_ = 0U;
  size_t header_position_ = 0U;
  size_t pcm_position_ = 0U;
  uint32_t stream_id_ = 0U;
  uint32_t sequence_ = 0U;
  uint32_t expected_crc32_ = 0U;
  uint8_t header_[PR1_USB_HEADER_BYTES] = {};
  uint8_t pcm_[PR1_PCM_BYTES_PER_FRAME] = {};
};

class Pr1FrameReassembler {
 public:
  Pr1FrameReassembler() {
    resetAll();
  }

  Pr1ReassemblyResult push(const uint8_t* packet,
                           size_t length,
                           Pr1CompletedFrame* completed,
                           bool* dropped_incomplete = nullptr,
                           bool* stream_changed = nullptr) {
    if (completed != nullptr) {
      completed->stream_id = 0U;
      completed->sequence = 0U;
      memset(completed->pcm, 0, sizeof(completed->pcm));
    }
    if (dropped_incomplete != nullptr) {
      *dropped_incomplete = false;
    }
    if (stream_changed != nullptr) {
      *stream_changed = false;
    }

    Pr1DecodedFragment fragment;
    if (!pr1_decode_rf_fragment(packet, length, &fragment)) {
      return Pr1ReassemblyResult::Rejected;
    }

    if (!has_stream_) {
      has_stream_ = true;
      stream_id_ = fragment.stream_id;
    } else if (fragment.stream_id != stream_id_) {
      if (has_retired_stream_ &&
          fragment.stream_id == retired_stream_id_) {
        return Pr1ReassemblyResult::Stale;
      }
      retired_stream_id_ = stream_id_;
      has_retired_stream_ = true;
      stream_id_ = fragment.stream_id;
      has_last_completed_ = false;
      last_completed_ = 0U;
      resetActive();
      if (stream_changed != nullptr) {
        *stream_changed = true;
      }
    }

    if (!active_) {
      if (has_last_completed_ &&
          !pr1_sequence_after(fragment.sequence, last_completed_)) {
        return Pr1ReassemblyResult::Stale;
      }
      startFrame(fragment);
    } else if (fragment.sequence != sequence_) {
      if (!pr1_sequence_after(fragment.sequence, sequence_)) {
        return Pr1ReassemblyResult::Stale;
      }
      if (received_mask_ != fullMask() && dropped_incomplete != nullptr) {
        *dropped_incomplete = true;
      }
      startFrame(fragment);
    } else if (fragment.frame_crc32 != frame_crc32_) {
      return Pr1ReassemblyResult::Rejected;
    }

    const uint8_t bit =
        static_cast<uint8_t>(1U << fragment.fragment_index);
    const size_t offset =
        static_cast<size_t>(fragment.fragment_index) *
        PR1_RF_FRAGMENT_DATA_BYTES;
    if ((received_mask_ & bit) != 0U) {
      return memcmp(&pcm_[offset],
                    fragment.payload,
                    fragment.payload_bytes) == 0
                 ? Pr1ReassemblyResult::Duplicate
                 : Pr1ReassemblyResult::Rejected;
    }

    memcpy(&pcm_[offset], fragment.payload, fragment.payload_bytes);
    received_mask_ = static_cast<uint8_t>(received_mask_ | bit);
    if (received_mask_ != fullMask()) {
      return Pr1ReassemblyResult::Accepted;
    }

    const uint32_t actual_crc32 =
        pr1_crc32(pcm_, PR1_PCM_BYTES_PER_FRAME);
    if (actual_crc32 != frame_crc32_) {
      resetActive();
      return Pr1ReassemblyResult::CrcFailed;
    }

    if (completed != nullptr) {
      completed->stream_id = stream_id_;
      completed->sequence = sequence_;
      memcpy(completed->pcm, pcm_, sizeof(completed->pcm));
    }
    last_completed_ = sequence_;
    has_last_completed_ = true;
    resetActive();
    return Pr1ReassemblyResult::Complete;
  }

  void resetAll() {
    active_ = false;
    has_stream_ = false;
    has_retired_stream_ = false;
    has_last_completed_ = false;
    stream_id_ = 0U;
    retired_stream_id_ = 0U;
    sequence_ = 0U;
    last_completed_ = 0U;
    frame_crc32_ = 0U;
    received_mask_ = 0U;
    memset(pcm_, 0, sizeof(pcm_));
  }

 private:
  static constexpr uint8_t fullMask() {
    return static_cast<uint8_t>((1U << PR1_RF_FRAGMENT_COUNT) - 1U);
  }

  void startFrame(const Pr1DecodedFragment& fragment) {
    active_ = true;
    sequence_ = fragment.sequence;
    frame_crc32_ = fragment.frame_crc32;
    received_mask_ = 0U;
    memset(pcm_, 0, sizeof(pcm_));
  }

  void resetActive() {
    active_ = false;
    sequence_ = 0U;
    frame_crc32_ = 0U;
    received_mask_ = 0U;
    memset(pcm_, 0, sizeof(pcm_));
  }

  bool active_ = false;
  bool has_stream_ = false;
  bool has_retired_stream_ = false;
  bool has_last_completed_ = false;
  uint32_t stream_id_ = 0U;
  uint32_t retired_stream_id_ = 0U;
  uint32_t sequence_ = 0U;
  uint32_t last_completed_ = 0U;
  uint32_t frame_crc32_ = 0U;
  uint8_t received_mask_ = 0U;
  uint8_t pcm_[PR1_PCM_BYTES_PER_FRAME] = {};
};
