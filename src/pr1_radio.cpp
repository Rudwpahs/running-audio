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

void pr1ConfigureRadio() {
  SPI.begin(PR1_RADIO_SCK, PR1_RADIO_MISO, PR1_RADIO_MOSI, PR1_RADIO_CS);

  ConfigFLRC_t config;
  config.frequency = PR1_FLRC_FREQUENCY_MHZ;
  pr1CheckRadioStep("beginFLRC", radio.beginFLRC(config));
  pr1CheckRadioStep("setFrequency",
                    radio.setFrequency(PR1_FLRC_FREQUENCY_MHZ));
  pr1CheckRadioStep("setBitRate",
                    radio.setBitRate(PR1_FLRC_BIT_RATE_KBPS));
  pr1CheckRadioStep("setCodingRate",
                    radio.setCodingRate(PR1_FLRC_CODING_RATE));
  pr1CheckRadioStep("setOutputPower",
                    radio.setOutputPower(PR1_RF_OUTPUT_DBM));
  pr1CheckRadioStep("setDataShaping",
                    radio.setDataShaping(PR1_FLRC_DATA_SHAPING));

  uint8_t sync_word[sizeof(PR1_RF_SYNC_WORD)];
  memcpy(sync_word, PR1_RF_SYNC_WORD, sizeof(sync_word));
  pr1CheckRadioStep("setSyncWord",
                    radio.setSyncWord(sync_word, sizeof(sync_word)));
}
