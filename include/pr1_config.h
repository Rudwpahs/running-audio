#pragma once

#include <stdint.h>

// LILYGO T3-S3 SX1280 radio wiring.
static constexpr int PR1_RADIO_SCK = 5;
static constexpr int PR1_RADIO_MISO = 3;
static constexpr int PR1_RADIO_MOSI = 6;
static constexpr int PR1_RADIO_CS = 7;
static constexpr int PR1_RADIO_RESET = 8;
static constexpr int PR1_RADIO_DIO1 = 9;
static constexpr int PR1_RADIO_BUSY = 36;
static constexpr int PR1_STATUS_LED = 37;

// MAX98357A wiring on the receiver.
static constexpr int PR1_I2S_BCLK = 43;
static constexpr int PR1_I2S_LRCLK = 44;
static constexpr int PR1_I2S_DOUT = 21;

static constexpr uint32_t PR1_SAMPLE_RATE = 16000;
static constexpr uint16_t PR1_SAMPLES_PER_PACKET = 120;
static constexpr uint16_t PR1_PCM_BYTES_PER_PACKET =
    PR1_SAMPLES_PER_PACKET * sizeof(int16_t);
static constexpr uint16_t PR1_RF_HEADER_BYTES = 12;
static constexpr uint16_t PR1_RF_PACKET_BYTES =
    PR1_RF_HEADER_BYTES + PR1_PCM_BYTES_PER_PACKET;
static constexpr uint8_t PR1_JITTER_START_PACKETS = 6;
static constexpr uint8_t PR1_JITTER_QUEUE_PACKETS = 16;

// Conservative limiter for the first bone-conduction test.
static constexpr int16_t PR1_MAX_ABS_SAMPLE = 4096;

static constexpr float PR1_FLRC_FREQUENCY_MHZ = 2410.5f;
static constexpr float PR1_FLRC_BIT_RATE_KBPS = 520.0f;
static constexpr uint8_t PR1_FLRC_CODING_RATE = 2;
static constexpr int8_t PR1_RF_OUTPUT_DBM = 3;
static constexpr float PR1_FLRC_DATA_SHAPING = 1.0f;

static constexpr uint8_t PR1_RF_SYNC_WORD[4] = {0x50, 0x52, 0x31, 0x32};
static constexpr uint8_t PR1_USB_MAGIC[4] = {'P', 'R', '1', 'U'};

static_assert(PR1_RF_PACKET_BYTES == 252,
              "The v2 radio packet must remain 252 bytes.");
static_assert(PR1_RF_PACKET_BYTES <= 255,
              "The packet must fit in one SX1280 frame.");
