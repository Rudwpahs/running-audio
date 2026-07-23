#include <Arduino.h>
#include <RadioLib.h>
#include <SPI.h>

#include "pr1_rf_protocol.h"

#ifndef PR1_ENABLE_RF_TEST
#define PR1_ENABLE_RF_TEST 0
#endif

#ifndef PR1_NODE_ROLE_TX
#define PR1_NODE_ROLE_TX 0
#endif

namespace {

// LILYGO T3-S3 MVSR + SX1280 pin map from the vendor pin_config.h.
constexpr int kRadioSclk = 5;
constexpr int kRadioMiso = 3;
constexpr int kRadioMosi = 6;
constexpr int kRadioCs = 7;
constexpr int kRadioRst = 8;
constexpr int kRadioDio1 = 9;
constexpr int kRadioBusy = 36;
constexpr int kRadioTxEnable = 10;
constexpr int kRadioRxEnable = 21;

// Align M0 with TECHNICAL_MVP T1: 100-byte deterministic RF data packet.
constexpr std::size_t kTestPacketBytes = 100;
constexpr std::size_t kTestPayloadBytes =
    kTestPacketBytes - pr1::kHeaderSize;
constexpr uint8_t kFlagTestPattern = 0x80;

static_assert(kTestPacketBytes <= 127,
              "M0 deterministic packet must fit the FLRC payload ceiling");

#if PR1_ENABLE_RF_TEST

#ifndef PR1_TEST_FREQ_MHZ
#error "Define PR1_TEST_FREQ_MHZ only after the exact test configuration is approved."
#endif

#ifndef PR1_TEST_POWER_DBM
#error "Define PR1_TEST_POWER_DBM only after the exact test configuration is approved."
#endif

constexpr float kTestFrequencyMHz = PR1_TEST_FREQ_MHZ;
constexpr int8_t kTestPowerDbm = PR1_TEST_POWER_DBM;
constexpr uint16_t kFlrcBitrateKbps = 650;
constexpr uint8_t kFlrcCodingRate = 3;  // RadioLib FLRC coding rate 3/4.
constexpr uint16_t kFlrcPreambleBits = 16;

SX1280 radio = new Module(
    kRadioCs, kRadioDio1, kRadioRst, kRadioBusy, SPI);

uint16_t g_packet_seq = 0;
uint16_t g_frame_seq = 0;
uint32_t g_tx_ok = 0;
uint32_t g_tx_fail = 0;
uint32_t g_rx_ok = 0;
uint32_t g_rx_bad_header = 0;
uint32_t g_rx_bad_pattern = 0;

void FillPattern(uint8_t* payload, std::size_t len, uint16_t packet_seq) {
  for (std::size_t i = 0; i < len; ++i) {
    payload[i] = static_cast<uint8_t>((packet_seq + i) & 0xFF);
  }
}

bool VerifyPattern(const uint8_t* payload, std::size_t len,
                   uint16_t packet_seq) {
  for (std::size_t i = 0; i < len; ++i) {
    if (payload[i] != static_cast<uint8_t>((packet_seq + i) & 0xFF)) {
      return false;
    }
  }
  return true;
}

bool InitRadio() {
  SPI.begin(kRadioSclk, kRadioMiso, kRadioMosi);

  Serial.printf(
      "PR1,radio_init,mode=FLRC,freq_mhz=%.3f,bitrate_kbps=%u,power_dbm=%d\n",
      static_cast<double>(kTestFrequencyMHz), kFlrcBitrateKbps,
      static_cast<int>(kTestPowerDbm));

  const int16_t state = radio.beginFLRC(
      kTestFrequencyMHz,
      kFlrcBitrateKbps,
      kFlrcCodingRate,
      kTestPowerDbm,
      kFlrcPreambleBits,
      RADIOLIB_SHAPING_0_5);

  if (state != RADIOLIB_ERR_NONE) {
    Serial.printf("PR1,radio_init_failed,state=%d\n", state);
    return false;
  }

  // Vendor pin_config.h defines LORA_RX=21 and LORA_TX=10 for SX1280.
  // RadioLib expects RX-enable first and TX-enable second.
  radio.setRfSwitchPins(kRadioRxEnable, kRadioTxEnable);

  Serial.println("PR1,radio_init_ok");
  return true;
}

void TransmitOne() {
  uint8_t packet[kTestPacketBytes]{};
  const pr1::Header header{
      kFlagTestPattern,
      g_packet_seq,
      g_frame_seq,
      0,
      1,
  };

  const pr1::Error header_state =
      pr1::EncodeHeader(header, packet, sizeof(packet));
  if (header_state != pr1::Error::kOk) {
    Serial.println("PR1,tx_internal_error,reason=header_encode");
    ++g_tx_fail;
    return;
  }

  FillPattern(packet + pr1::kHeaderSize, kTestPayloadBytes, g_packet_seq);

  const uint32_t started_us = micros();
  const int16_t state = radio.transmit(packet, sizeof(packet));
  const uint32_t elapsed_us = micros() - started_us;

  if (state == RADIOLIB_ERR_NONE) {
    ++g_tx_ok;
    Serial.printf(
        "PR1,tx_ok,packet_seq=%u,frame_seq=%u,len=%u,elapsed_us=%lu,ok=%lu,fail=%lu\n",
        g_packet_seq,
        g_frame_seq,
        static_cast<unsigned>(sizeof(packet)),
        static_cast<unsigned long>(elapsed_us),
        static_cast<unsigned long>(g_tx_ok),
        static_cast<unsigned long>(g_tx_fail));
  } else {
    ++g_tx_fail;
    Serial.printf(
        "PR1,tx_fail,packet_seq=%u,state=%d,elapsed_us=%lu,ok=%lu,fail=%lu\n",
        g_packet_seq,
        state,
        static_cast<unsigned long>(elapsed_us),
        static_cast<unsigned long>(g_tx_ok),
        static_cast<unsigned long>(g_tx_fail));
  }

  ++g_packet_seq;
  ++g_frame_seq;
}

void ReceiveOne() {
  uint8_t packet[kTestPacketBytes]{};
  const int16_t state = radio.receive(packet, sizeof(packet));

  if (state != RADIOLIB_ERR_NONE) {
    Serial.printf("PR1,rx_radio_error,state=%d\n", state);
    return;
  }

  pr1::Header header{};
  const pr1::Error header_state =
      pr1::DecodeHeader(packet, sizeof(packet), &header);
  if (header_state != pr1::Error::kOk) {
    ++g_rx_bad_header;
    Serial.printf(
        "PR1,rx_bad_header,count=%lu,rssi=%.1f,snr=%.1f\n",
        static_cast<unsigned long>(g_rx_bad_header),
        static_cast<double>(radio.getRSSI()),
        static_cast<double>(radio.getSNR()));
    return;
  }

  if (!VerifyPattern(packet + pr1::kHeaderSize,
                     kTestPayloadBytes,
                     header.packet_seq)) {
    ++g_rx_bad_pattern;
    Serial.printf(
        "PR1,rx_bad_pattern,packet_seq=%u,count=%lu,rssi=%.1f,snr=%.1f\n",
        header.packet_seq,
        static_cast<unsigned long>(g_rx_bad_pattern),
        static_cast<double>(radio.getRSSI()),
        static_cast<double>(radio.getSNR()));
    return;
  }

  ++g_rx_ok;
  Serial.printf(
      "PR1,rx_ok,packet_seq=%u,frame_seq=%u,len=%u,rssi=%.1f,snr=%.1f,ok=%lu,bad_header=%lu,bad_pattern=%lu\n",
      header.packet_seq,
      header.frame_seq,
      static_cast<unsigned>(sizeof(packet)),
      static_cast<double>(radio.getRSSI()),
      static_cast<double>(radio.getSNR()),
      static_cast<unsigned long>(g_rx_ok),
      static_cast<unsigned long>(g_rx_bad_header),
      static_cast<unsigned long>(g_rx_bad_pattern));
}

#endif  // PR1_ENABLE_RF_TEST

void PrintBuildConfiguration() {
  Serial.println();
  Serial.println("PR1 M0 SX1280 deterministic packet link");
  Serial.printf("PR1,protocol_version=%u,header_bytes=%u,test_packet_bytes=%u\n",
                pr1::kVersion,
                static_cast<unsigned>(pr1::kHeaderSize),
                static_cast<unsigned>(kTestPacketBytes));
  Serial.printf(
      "PR1,pins,sclk=%d,miso=%d,mosi=%d,cs=%d,rst=%d,dio1=%d,busy=%d,tx_en=%d,rx_en=%d\n",
      kRadioSclk,
      kRadioMiso,
      kRadioMosi,
      kRadioCs,
      kRadioRst,
      kRadioDio1,
      kRadioBusy,
      kRadioTxEnable,
      kRadioRxEnable);
  Serial.printf("PR1,rf_enabled=%d,node_role=%s\n",
                PR1_ENABLE_RF_TEST,
                PR1_NODE_ROLE_TX ? "TX" : "RX");
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(1000);
  PrintBuildConfiguration();

#if PR1_ENABLE_RF_TEST
  if (!InitRadio()) {
    Serial.println("PR1,halt,reason=radio_init_failed");
    while (true) {
      delay(1000);
    }
  }
#else
  Serial.println(
      "PR1,RF_DISABLED: safe build only; no SX1280 initialization/transmission");
#endif
}

void loop() {
#if PR1_ENABLE_RF_TEST
#if PR1_NODE_ROLE_TX
  TransmitOne();
  delay(100);  // 10 packets/s baseline. Audio-rate traffic comes later.
#else
  ReceiveOne();
#endif
#else
  delay(1000);
#endif
}
