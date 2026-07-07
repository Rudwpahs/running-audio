#if defined(PR1_ROLE_RX)

#include <Arduino.h>
#include <esp_err.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#include <driver/i2s.h>

#include "pr1_config.h"
#include "pr1_protocol.h"
#include "pr1_radio.h"

struct AudioPacket {
  uint32_t sequence;
  int16_t samples[PR1_SAMPLES_PER_PACKET];
};

static QueueHandle_t audio_queue = nullptr;
static volatile bool radio_packet_ready = false;
static portMUX_TYPE stats_mux = portMUX_INITIALIZER_UNLOCKED;
static uint32_t rx_packets = 0;
static uint32_t rx_invalid = 0;
static uint32_t rx_queue_drops = 0;
static uint32_t rx_missing = 0;
static uint32_t rx_late = 0;
static uint32_t last_report_ms = 0;

#if defined(ESP32)
IRAM_ATTR
#endif
static void onRadioPacket() {
  radio_packet_ready = true;
}

static void addStat(uint32_t* value, uint32_t amount = 1U) {
  portENTER_CRITICAL(&stats_mux);
  *value += amount;
  portEXIT_CRITICAL(&stats_mux);
}

static int16_t clampSample(int16_t sample) {
  if (sample > PR1_MAX_ABS_SAMPLE) {
    return PR1_MAX_ABS_SAMPLE;
  }
  if (sample < -PR1_MAX_ABS_SAMPLE) {
    return -PR1_MAX_ABS_SAMPLE;
  }
  return sample;
}

static void writeSamples(const int16_t* mono) {
  int16_t stereo[PR1_SAMPLES_PER_PACKET * 2];
  for (size_t i = 0; i < PR1_SAMPLES_PER_PACKET; ++i) {
    const int16_t value = clampSample(mono[i]);
    stereo[2 * i] = value;
    stereo[2 * i + 1] = value;
  }

  size_t bytes_written = 0;
  i2s_write(I2S_NUM_0,
            stereo,
            sizeof(stereo),
            &bytes_written,
            portMAX_DELAY);
}

static void writeSilence() {
  static const int16_t silence[PR1_SAMPLES_PER_PACKET] = {};
  writeSamples(silence);
}

static void audioOutputTask(void*) {
  while (uxQueueMessagesWaiting(audio_queue) < PR1_JITTER_START_PACKETS) {
    vTaskDelay(pdMS_TO_TICKS(2));
  }

  bool sequence_initialized = false;
  uint32_t expected_sequence = 0;

  while (true) {
    AudioPacket packet;
    if (xQueueReceive(audio_queue, &packet, portMAX_DELAY) != pdTRUE) {
      continue;
    }

    if (!sequence_initialized) {
      expected_sequence = packet.sequence;
      sequence_initialized = true;
    }

    if (packet.sequence == expected_sequence) {
      writeSamples(packet.samples);
      ++expected_sequence;
      continue;
    }

    if (pr1_sequence_after(packet.sequence, expected_sequence)) {
      const uint32_t gap = packet.sequence - expected_sequence;
      addStat(&rx_missing, gap);
      const uint32_t silence_frames = (gap > 8U) ? 8U : gap;
      for (uint32_t i = 0; i < silence_frames; ++i) {
        writeSilence();
      }
      writeSamples(packet.samples);
      expected_sequence = packet.sequence + 1U;
    } else {
      addStat(&rx_late);
    }
  }
}

static void configureI2s() {
  i2s_config_t config = {};
  config.mode = static_cast<i2s_mode_t>(I2S_MODE_MASTER | I2S_MODE_TX);
  config.sample_rate = PR1_SAMPLE_RATE;
  config.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT;
  config.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT;
  config.communication_format = I2S_COMM_FORMAT_STAND_I2S;
  config.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1;
  config.dma_buf_count = 8;
  config.dma_buf_len = PR1_SAMPLES_PER_PACKET;
  config.use_apll = false;
  config.tx_desc_auto_clear = true;
  config.fixed_mclk = 0;

  i2s_pin_config_t pins = {};
  pins.mck_io_num = I2S_PIN_NO_CHANGE;
  pins.bck_io_num = PR1_I2S_BCLK;
  pins.ws_io_num = PR1_I2S_LRCLK;
  pins.data_out_num = PR1_I2S_DOUT;
  pins.data_in_num = I2S_PIN_NO_CHANGE;

  ESP_ERROR_CHECK(i2s_driver_install(I2S_NUM_0, &config, 0, nullptr));
  ESP_ERROR_CHECK(i2s_set_pin(I2S_NUM_0, &pins));
  ESP_ERROR_CHECK(i2s_zero_dma_buffer(I2S_NUM_0));
}

static void discardCurrentPacket(size_t length) {
  uint8_t discard[255] = {};
  if (length > 0U) {
    const size_t safe_length = (length > sizeof(discard)) ? sizeof(discard) : length;
    radio.readData(discard, safe_length);
  }
}

static void handleReceivedPacket() {
  radio_packet_ready = false;

  const size_t length = radio.getPacketLength();
  if (length != PR1_RF_PACKET_BYTES) {
    addStat(&rx_invalid);
    discardCurrentPacket(length);
    radio.startReceive();
    return;
  }

  uint8_t raw[PR1_RF_PACKET_BYTES] = {};
  const int16_t state = radio.readData(raw, sizeof(raw));
  if (state != RADIOLIB_ERR_NONE) {
    addStat(&rx_invalid);
    radio.startReceive();
    return;
  }

  Pr1DecodedPacket decoded;
  if (!pr1_decode_rf_packet(raw, sizeof(raw), &decoded)) {
    addStat(&rx_invalid);
    radio.startReceive();
    return;
  }

  AudioPacket packet;
  packet.sequence = decoded.sequence;
  memcpy(packet.samples, decoded.samples, PR1_PCM_BYTES_PER_PACKET);
  if (xQueueSend(audio_queue, &packet, 0) != pdTRUE) {
    addStat(&rx_queue_drops);
  } else {
    addStat(&rx_packets);
    digitalWrite(PR1_STATUS_LED, !digitalRead(PR1_STATUS_LED));
  }

  radio.startReceive();
}

void setupRole() {
  Serial.begin(PR1_LOG_BAUD);
  const uint32_t start_ms = millis();
  while (!Serial && (millis() - start_ms) < 3000U) {
    delay(10);
  }

  configureI2s();
  audio_queue = xQueueCreate(PR1_JITTER_QUEUE_PACKETS, sizeof(AudioPacket));
  if (audio_queue == nullptr) {
    pr1HaltWithCode("xQueueCreate", -1);
  }

  pr1ConfigureRadio();
  radio.setPacketReceivedAction(onRadioPacket);
  pr1CheckRadioStep("startReceive", radio.startReceive());

  const BaseType_t created = xTaskCreatePinnedToCore(
      audioOutputTask, "pr1_audio", 4096, nullptr, 4, nullptr, 0);
  if (created != pdPASS) {
    pr1HaltWithCode("xTaskCreatePinnedToCore", -2);
  }

  Serial.println("[PR1RX] READY");
}

void loopRole() {
  if (radio_packet_ready) {
    handleReceivedPacket();
  }

  const uint32_t now = millis();
  if (now - last_report_ms >= 1000U) {
    last_report_ms = now;

    portENTER_CRITICAL(&stats_mux);
    const uint32_t packets = rx_packets;
    const uint32_t invalid = rx_invalid;
    const uint32_t queue_drops = rx_queue_drops;
    const uint32_t missing = rx_missing;
    const uint32_t late = rx_late;
    portEXIT_CRITICAL(&stats_mux);

    Serial.printf(
        "[PR1RX] received=%lu invalid=%lu queue_drop=%lu "
        "missing=%lu late=%lu queued=%u rssi=%.1f\n",
        static_cast<unsigned long>(packets),
        static_cast<unsigned long>(invalid),
        static_cast<unsigned long>(queue_drops),
        static_cast<unsigned long>(missing),
        static_cast<unsigned long>(late),
        static_cast<unsigned>(uxQueueMessagesWaiting(audio_queue)),
        radio.getRSSI());
  }
  delay(1);
}

#endif
