#if defined(PR1_ROLE_TX)

#include <Arduino.h>

#include "pr1_config.h"
#include "pr1_protocol.h"
#include "pr1_radio.h"

static Pr1UsbStreamParser usb_parser;
static uint8_t rf_packet[PR1_RF_MAX_PACKET_BYTES] = {};
static uint64_t tx_frames = 0;
static uint64_t tx_fragments = 0;
static uint64_t tx_errors = 0;
static uint64_t usb_header_errors = 0;
static uint64_t usb_crc_errors = 0;
static uint32_t last_report_ms = 0;

static void transmitUsbFrame(const Pr1UsbFrameView& frame) {
  bool frame_ok = true;
  for (uint8_t fragment_index = 0;
       fragment_index < PR1_RF_FRAGMENT_COUNT;
       ++fragment_index) {
    const size_t packet_length = pr1_encode_rf_fragment(
        rf_packet,
        sizeof(rf_packet),
        frame.stream_id,
        frame.sequence,
        frame.frame_crc32,
        frame.pcm,
        fragment_index);
    if (packet_length == 0U ||
        packet_length > PR1_RF_MAX_PACKET_BYTES) {
      ++tx_errors;
      frame_ok = false;
      break;
    }

    const int16_t state = radio.transmit(rf_packet, packet_length);
    if (state != RADIOLIB_ERR_NONE) {
      ++tx_errors;
      frame_ok = false;
      break;
    }
    ++tx_fragments;
  }

  if (frame_ok) {
    ++tx_frames;
    digitalWrite(PR1_STATUS_LED, !digitalRead(PR1_STATUS_LED));
  }
}

static void consumeUsbByte(uint8_t value) {
  Pr1UsbFrameView frame;
  const Pr1UsbParserResult result = usb_parser.push(value, &frame);
  switch (result) {
    case Pr1UsbParserResult::FrameReady:
      transmitUsbFrame(frame);
      break;
    case Pr1UsbParserResult::HeaderRejected:
      ++usb_header_errors;
      break;
    case Pr1UsbParserResult::CrcRejected:
      ++usb_crc_errors;
      break;
    case Pr1UsbParserResult::None:
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
  Serial.println("[PR1TX] READY protocol=3 frame_ms=10 fragments=3");
}

void loopRole() {
  while (Serial.available() > 0) {
    consumeUsbByte(static_cast<uint8_t>(Serial.read()));
  }

  const uint32_t now = millis();
  if (now - last_report_ms >= 1000U) {
    last_report_ms = now;
    Serial.printf(
        "[PR1TX] frames=%llu fragments=%llu rf_errors=%llu "
        "usb_header_errors=%llu usb_crc_errors=%llu buffered=%d\n",
        static_cast<unsigned long long>(tx_frames),
        static_cast<unsigned long long>(tx_fragments),
        static_cast<unsigned long long>(tx_errors),
        static_cast<unsigned long long>(usb_header_errors),
        static_cast<unsigned long long>(usb_crc_errors),
        Serial.available());
  }
  delay(1);
}

#endif
