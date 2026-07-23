#include "pr1_radio.h"

#include <Arduino.h>
#include <SPI.h>

#include "pr1_config.h"

SX1280 radio = new Module(
    PR1_RADIO_CS, PR1_RADIO_DIO1, PR1_RADIO_RESET, PR1_RADIO_BUSY);

void pr1HaltWithCode(const char* step, int16_t code) {
  Serial.printf("[PR1] %s failed: %d\n", step, code);
  while (true) {
    digitalWrite(PR1_STATUS_LED, !digitalRead(PR1_STATUS_LED));
    delay(250);
  }
}

void pr1CheckRadioStep(const char* step, int16_t code) {
  if (code != RADIOLIB_ERR_NONE) {
    pr1HaltWithCode(step, code);
  }
}

int16_t pr1StartSingleReceive() {
  // SX1280 errata 16.1 warns that continuous RX can lock up when packet
  // traffic exceeds roughly 220 packets/s. PR1 produces 300 fragments/s, so
  // use SetRx(0) (single mode) and explicitly restart after every packet.
  return radio.startReceive(RADIOLIB_SX128X_RX_TIMEOUT_NONE);
}

void pr1ConfigureRadio() {
  SPI.begin(PR1_RADIO_SCK, PR1_RADIO_MISO, PR1_RADIO_MOSI, PR1_RADIO_CS);

  ConfigFLRC_t config;
  config.frequency = PR1_FLRC_FREQUENCY_MHZ;
  config.bitRate = PR1_FLRC_BIT_RATE_KBPS;
  config.codingRate = PR1_FLRC_CODING_RATE;
  config.power = PR1_RF_OUTPUT_DBM;
  config.preambleLength = 16;
  config.dataShaping = RADIOLIB_SHAPING_0_5;
  pr1CheckRadioStep("beginFLRC", radio.beginFLRC(config));

  // H594/H594-01 route the SX1280 RF path through GPIO-controlled
  // receive/transmit switches. Omitting this can produce a clean build but
  // no usable physical link.
  radio.setRfSwitchPins(PR1_RADIO_RX_ENABLE, PR1_RADIO_TX_ENABLE);

  uint8_t sync_word[sizeof(PR1_RF_SYNC_WORD)];
  memcpy(sync_word, PR1_RF_SYNC_WORD, sizeof(sync_word));
  pr1CheckRadioStep("setSyncWord",
                    radio.setSyncWord(sync_word, sizeof(sync_word)));
  pr1CheckRadioStep("setCRC", radio.setCRC(2));
  pr1CheckRadioStep("setEncoding",
                    radio.setEncoding(RADIOLIB_ENCODING_NRZ));
  pr1CheckRadioStep(
      "variablePacketLengthMode",
      radio.variablePacketLengthMode(PR1_RF_MAX_PACKET_BYTES));
}
