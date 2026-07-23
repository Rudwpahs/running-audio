#pragma once

#include <stdint.h>

// LILYGO T3-S3 V1.2 SX1280 wiring from the official LILYGO examples.
static constexpr int PR1_RADIO_SCK = 5;
static constexpr int PR1_RADIO_MISO = 3;
static constexpr int PR1_RADIO_MOSI = 6;
static constexpr int PR1_RADIO_CS = 7;
static constexpr int PR1_RADIO_RESET = 8;
static constexpr int PR1_RADIO_DIO1 = 9;
static constexpr int PR1_RADIO_BUSY = 36;
static constexpr int PR1_RADIO_TX_ENABLE = 10;
static constexpr int PR1_RADIO_RX_ENABLE = 21;
static constexpr int PR1_STATUS_LED = 37;

// Onboard MAX98357A on the T3-S3 MVSR daughterboard.
static constexpr int PR1_I2S_BCLK = 40;
static constexpr int PR1_I2S_LRCLK = 41;
static constexpr int PR1_I2S_DOUT = 39;
static constexpr int PR1_I2S_AMP_ENABLE = 38;

static constexpr uint32_t PR1_SAMPLE_RATE = 16000;
static constexpr uint16_t PR1_AUDIO_FRAME_MS = 10;
static constexpr uint16_t PR1_SAMPLES_PER_FRAME = 160;
static constexpr uint16_t PR1_PCM_BYTES_PER_FRAME =
    PR1_SAMPLES_PER_FRAME * sizeof(int16_t);

// SX1280 FLRC packetParam5 is limited to 6..127 bytes by the datasheet.
static constexpr uint16_t PR1_RF_MAX_PACKET_BYTES = 127;
static constexpr uint16_t PR1_RF_HEADER_BYTES = 19;
static constexpr uint16_t PR1_RF_FRAGMENT_DATA_BYTES = 108;
static constexpr uint8_t PR1_RF_FRAGMENT_COUNT = 3;

static constexpr uint16_t PR1_USB_HEADER_BYTES = 20;
static constexpr uint16_t PR1_USB_FRAME_BYTES =
    PR1_USB_HEADER_BYTES + PR1_PCM_BYTES_PER_FRAME;

static constexpr uint8_t PR1_JITTER_START_FRAMES = 4;
static constexpr uint8_t PR1_JITTER_QUEUE_FRAMES = 12;

// Conservative limiter for the first bone-conduction test.
static constexpr int16_t PR1_MAX_ABS_SAMPLE = 4096;

static constexpr float PR1_FLRC_FREQUENCY_MHZ = 2410.5f;
static constexpr float PR1_FLRC_BIT_RATE_KBPS = 650.0f;
// RadioLib's SX128x API maps 3 to the FLRC 3/4 coding rate.
static constexpr uint8_t PR1_FLRC_CODING_RATE = 3;
static constexpr int8_t PR1_RF_OUTPUT_DBM = 3;

// "PRP1". For FLRC CR 3/4, Semtech forbids MSW 0x8C38/0x630E and
// LSW 0x0000..0x3EFF. 0x5052_5031 stays outside those errata ranges.
static constexpr uint8_t PR1_RF_SYNC_WORD[4] = {0x50, 0x52, 0x50, 0x31};
static constexpr uint8_t PR1_USB_MAGIC[4] = {'P', 'R', '1', 'U'};
static constexpr uint16_t PR1_RF_SYNC_MSW =
    (static_cast<uint16_t>(PR1_RF_SYNC_WORD[0]) << 8) |
    PR1_RF_SYNC_WORD[1];
static constexpr uint16_t PR1_RF_SYNC_LSW =
    (static_cast<uint16_t>(PR1_RF_SYNC_WORD[2]) << 8) |
    PR1_RF_SYNC_WORD[3];

static_assert(PR1_SAMPLE_RATE * PR1_AUDIO_FRAME_MS / 1000U ==
                  PR1_SAMPLES_PER_FRAME,
              "Audio frame duration and sample count must agree.");
static_assert(PR1_RF_HEADER_BYTES + PR1_RF_FRAGMENT_DATA_BYTES ==
                  PR1_RF_MAX_PACKET_BYTES,
              "A full fragment must fit the FLRC 127-byte limit.");
static_assert(
    (PR1_PCM_BYTES_PER_FRAME + PR1_RF_FRAGMENT_DATA_BYTES - 1U) /
            PR1_RF_FRAGMENT_DATA_BYTES ==
        PR1_RF_FRAGMENT_COUNT,
    "The 10 ms PCM frame must require exactly three RF fragments.");
static_assert(PR1_RF_SYNC_MSW != 0x8C38U &&
                  PR1_RF_SYNC_MSW != 0x630EU,
              "FLRC CR 3/4 errata forbids this sync-word MSW.");
static_assert(PR1_RF_SYNC_LSW > 0x3EFFU,
              "FLRC CR 3/4 errata forbids this sync-word LSW.");
