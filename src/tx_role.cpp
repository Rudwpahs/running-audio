#if defined(PR1_ROLE_TX)

#include <Arduino.h>

#include "pr1_config.h"
#include "pr1_protocol.h"
#include "pr1_radio.h"

enum class UsbParserState : uint8_t {
  FindMagic,
  ReadSequence,
  ReadPcm,
};

static UsbParserState parser_state = UsbParserState::FindMagic;
static uint8_t magic_position = 0;
static uint8_t sequence_bytes[4] = {};
static size_t sequence_position = 0;
static int16_t pcm_samples[PR1_SAMPLES_PER_PACKET] = {};
static size_t pcm_byte_position = 0;
static uint8_t rf_packet[PR1_RF_PACKET_BYTES] = {};
static uint64_t tx_packets = 0;
static uint64_t tx_errors = 0;
static uint32_t last_report_ms = 0;

static void transmitCurrentUsbFrame() {
  const uint32_t sequence = pr1_read_le32(sequence_bytes);
  pr1_encode_rf_packet(rf_packet, sequence, pcm_samples);

  const int16_t state = radio.transmit(rf_packet, sizeof(rf_packet));
  if (state == RADIOLIB_ERR_NONE) {
    ++tx_packets;
    digitalWrite(PR1_STATUS_LED, !digitalRead(PR1_STATUS_LED));
  } else {
    ++tx_errors;
  }
}

static void consumeUsbByte(uint8_t value) {
  switch (parser_state) {
    case UsbParserState::FindMagic:
      if (value == PR1_USB_MAGIC[magic_position]) {
        ++magic_position;
        if (magic_position == sizeof(PR1_USB_MAGIC)) {
          parser_state = UsbParserState::ReadSequence;
          magic_position = 0;
          sequence_position = 0;
        }
      } else {
        magic_position = (value == PR1_USB_MAGIC[0]) ? 1 : 0;
      }
      break;

    case UsbParserState::ReadSequence:
      sequence_bytes[sequence_position++] = value;
      if (sequence_position == sizeof(sequence_bytes)) {
        parser_state = UsbParserState::ReadPcm;
        pcm_byte_position = 0;
      }
      break;

    case UsbParserState::ReadPcm:
      reinterpret_cast<uint8_t*>(pcm_samples)[pcm_byte_position++] = value;
      if (pcm_byte_position == PR1_PCM_BYTES_PER_PACKET) {
        transmitCurrentUsbFrame();
        parser_state = UsbParserState::FindMagic;
        magic_position = 0;
      }
      break;
  }
}

void setupRole() {
  Serial.setRxBufferSize(16384);
  Serial.begin(PR1_SERIAL_BAUD);
  const uint32_t start_ms = millis();
  while (!Serial && (millis() - start_ms) < 3000U) {
    delay(10);
  }

  pr1ConfigureRadio();
  Serial.println("[PR1TX] READY");
}

void loopRole() {
  while (Serial.available() > 0) {
    consumeUsbByte(static_cast<uint8_t>(Serial.read()));
  }

  const uint32_t now = millis();
  if (now - last_report_ms >= 1000U) {
    last_report_ms = now;
    Serial.printf("[PR1TX] sent=%llu errors=%llu buffered=%d\n",
                  static_cast<unsigned long long>(tx_packets),
                  static_cast<unsigned long long>(tx_errors),
                  Serial.available());
  }
  delay(1);
}

#endif
