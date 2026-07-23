#if defined(PR1_ROLE_RX)

#include <Arduino.h>
#include <driver/i2s.h>
#include <esp_err.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>

#include "pr1_config.h"
#include "pr1_protocol.h"
#include "pr1_radio.h"

struct AudioFrame {
  uint32_t stream_id;
  uint32_t sequence;
  int16_t samples[PR1_SAMPLES_PER_FRAME];
};

static QueueHandle_t audio_queue = nullptr;
static Pr1FrameReassembler reassembler;
static volatile bool radio_packet_ready = false;
static portMUX_TYPE stats_mux = portMUX_INITIALIZER_UNLOCKED;
static uint32_t rx_fragments = 0;
static uint32_t rx_frames = 0;
static uint32_t rx_invalid = 0;
static uint32_t rx_crc_failures = 0;
static uint32_t rx_duplicates = 0;
static uint32_t rx_incomplete = 0;
static uint32_t rx_queue_drops = 0;
static uint32_t rx_late = 0;
static uint32_t rx_underruns = 0;
static uint32_t rx_stream_changes = 0;
static uint32_t rx_stream_generation = 0;
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
  int16_t stereo[PR1_SAMPLES_PER_FRAME * 2];
  for (size_t i = 0; i < PR1_SAMPLES_PER_FRAME; ++i) {
    const int16_t value = clampSample(mono[i]);
    stereo[2U * i] = value;
    stereo[2U * i + 1U] = value;
  }

  size_t bytes_written = 0;
  i2s_write(I2S_NUM_1,
            stereo,
            sizeof(stereo),
            &bytes_written,
            portMAX_DELAY);
}

static void writeSilence() {
  static const int16_t silence[PR1_SAMPLES_PER_FRAME] = {};
  writeSamples(silence);
}

static void audioOutputTask(void*) {
  while (uxQueueMessagesWaiting(audio_queue) <
         PR1_JITTER_START_FRAMES) {
    vTaskDelay(pdMS_TO_TICKS(2));
  }

  AudioFrame pending;
  if (xQueueReceive(audio_queue, &pending, portMAX_DELAY) != pdTRUE) {
    vTaskDelete(nullptr);
    return;
  }

  bool has_pending = true;
  bool stream_initialized = true;
  uint32_t current_stream_id = pending.stream_id;
  uint32_t expected_sequence = pending.sequence;
  uint32_t seen_stream_generation = 0U;
  portENTER_CRITICAL(&stats_mux);
  seen_stream_generation = rx_stream_generation;
  portEXIT_CRITICAL(&stats_mux);

  while (true) {
    uint32_t current_generation = 0U;
    portENTER_CRITICAL(&stats_mux);
    current_generation = rx_stream_generation;
    portEXIT_CRITICAL(&stats_mux);
    if (current_generation != seen_stream_generation) {
      seen_stream_generation = current_generation;
      has_pending = false;
      stream_initialized = false;
    }

    if (!has_pending) {
      while (xQueueReceive(audio_queue, &pending, 0) == pdTRUE) {
        if (!stream_initialized ||
            pending.stream_id != current_stream_id) {
          current_stream_id = pending.stream_id;
          expected_sequence = pending.sequence;
          stream_initialized = true;
          has_pending = true;
          break;
        }
        if (pending.sequence == expected_sequence ||
            pr1_sequence_after(pending.sequence, expected_sequence)) {
          has_pending = true;
          break;
        }
        addStat(&rx_late);
      }
    }

    if (has_pending &&
        pending.stream_id == current_stream_id &&
        pending.sequence == expected_sequence) {
      writeSamples(pending.samples);
      has_pending = false;
    } else {
      // I2S output is the real-time clock. One silent 10 ms frame preserves
      // timing instead of pausing playback or bursting several frames later.
      writeSilence();
      addStat(&rx_underruns);
    }
    ++expected_sequence;
  }
}

static void configureI2s() {
  pinMode(PR1_I2S_AMP_ENABLE, OUTPUT);
  digitalWrite(PR1_I2S_AMP_ENABLE, HIGH);

  i2s_config_t config = {};
  config.mode = static_cast<i2s_mode_t>(I2S_MODE_MASTER | I2S_MODE_TX);
  config.sample_rate = PR1_SAMPLE_RATE;
  config.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT;
  config.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT;
  config.communication_format = I2S_COMM_FORMAT_STAND_I2S;
  config.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1;
  config.dma_buf_count = 8;
  config.dma_buf_len = PR1_SAMPLES_PER_FRAME;
  config.use_apll = false;
  config.tx_desc_auto_clear = true;
  config.fixed_mclk = 0;

  i2s_pin_config_t pins = {};
  pins.mck_io_num = I2S_PIN_NO_CHANGE;
  pins.bck_io_num = PR1_I2S_BCLK;
  pins.ws_io_num = PR1_I2S_LRCLK;
  pins.data_out_num = PR1_I2S_DOUT;
  pins.data_in_num = I2S_PIN_NO_CHANGE;

  ESP_ERROR_CHECK(i2s_driver_install(I2S_NUM_1, &config, 0, nullptr));
  ESP_ERROR_CHECK(i2s_set_pin(I2S_NUM_1, &pins));
  ESP_ERROR_CHECK(i2s_zero_dma_buffer(I2S_NUM_1));
}

static void discardCurrentPacket(size_t length) {
  uint8_t discard[PR1_RF_MAX_PACKET_BYTES] = {};
  if (length > 0U) {
    const size_t safe_length =
        (length > sizeof(discard)) ? sizeof(discard) : length;
    radio.readData(discard, safe_length);
  }
}

static void handleReceivedFragment() {
  radio_packet_ready = false;

  const size_t length = radio.getPacketLength();
  if (length < PR1_RF_HEADER_BYTES ||
      length > PR1_RF_MAX_PACKET_BYTES) {
    addStat(&rx_invalid);
    discardCurrentPacket(length);
    pr1CheckRadioStep("resumeReceive", pr1StartSingleReceive());
    return;
  }

  uint8_t raw[PR1_RF_MAX_PACKET_BYTES] = {};
  const int16_t state = radio.readData(raw, length);
  if (state != RADIOLIB_ERR_NONE) {
    addStat(&rx_invalid);
    pr1CheckRadioStep("resumeReceive", pr1StartSingleReceive());
    return;
  }

  Pr1CompletedFrame completed;
  bool dropped_incomplete = false;
  bool stream_changed = false;
  const Pr1ReassemblyResult result = reassembler.push(
      raw,
      length,
      &completed,
      &dropped_incomplete,
      &stream_changed);
  if (stream_changed) {
    xQueueReset(audio_queue);
    portENTER_CRITICAL(&stats_mux);
    ++rx_stream_changes;
    ++rx_stream_generation;
    portEXIT_CRITICAL(&stats_mux);
  }
  if (dropped_incomplete) {
    addStat(&rx_incomplete);
  }

  switch (result) {
    case Pr1ReassemblyResult::Accepted:
      addStat(&rx_fragments);
      break;
    case Pr1ReassemblyResult::Duplicate:
      addStat(&rx_fragments);
      addStat(&rx_duplicates);
      break;
    case Pr1ReassemblyResult::Complete: {
      addStat(&rx_fragments);
      AudioFrame frame;
      frame.stream_id = completed.stream_id;
      frame.sequence = completed.sequence;
      memcpy(frame.samples, completed.pcm, PR1_PCM_BYTES_PER_FRAME);
      if (xQueueSend(audio_queue, &frame, 0) != pdTRUE) {
        addStat(&rx_queue_drops);
      } else {
        addStat(&rx_frames);
        digitalWrite(PR1_STATUS_LED, !digitalRead(PR1_STATUS_LED));
      }
      break;
    }
    case Pr1ReassemblyResult::CrcFailed:
      addStat(&rx_fragments);
      addStat(&rx_crc_failures);
      break;
    case Pr1ReassemblyResult::Stale:
      addStat(&rx_late);
      break;
    case Pr1ReassemblyResult::Rejected:
      addStat(&rx_invalid);
      break;
  }

  pr1CheckRadioStep("resumeReceive", pr1StartSingleReceive());
}

void setupRole() {
  Serial.begin(PR1_LOG_BAUD);
  const uint32_t start_ms = millis();
  while (!Serial && (millis() - start_ms) < 3000U) {
    delay(10);
  }

  configureI2s();
  audio_queue =
      xQueueCreate(PR1_JITTER_QUEUE_FRAMES, sizeof(AudioFrame));
  if (audio_queue == nullptr) {
    pr1HaltWithCode("xQueueCreate", -1);
  }

  pr1ConfigureRadio();
  radio.setPacketReceivedAction(onRadioPacket);
  pr1CheckRadioStep("startReceive", pr1StartSingleReceive());

  const BaseType_t created = xTaskCreatePinnedToCore(
      audioOutputTask, "pr1_audio", 4096, nullptr, 4, nullptr, 0);
  if (created != pdPASS) {
    pr1HaltWithCode("xTaskCreatePinnedToCore", -2);
  }

  Serial.println("[PR1RX] READY protocol=3 onboard_amp=enabled");
}

void loopRole() {
  if (radio_packet_ready) {
    handleReceivedFragment();
  }

  const uint32_t now = millis();
  if (now - last_report_ms >= 1000U) {
    last_report_ms = now;

    portENTER_CRITICAL(&stats_mux);
    const uint32_t fragments = rx_fragments;
    const uint32_t frames = rx_frames;
    const uint32_t invalid = rx_invalid;
    const uint32_t crc_failures = rx_crc_failures;
    const uint32_t duplicates = rx_duplicates;
    const uint32_t incomplete = rx_incomplete;
    const uint32_t queue_drops = rx_queue_drops;
    const uint32_t late = rx_late;
    const uint32_t underruns = rx_underruns;
    const uint32_t stream_changes = rx_stream_changes;
    portEXIT_CRITICAL(&stats_mux);

    Serial.printf(
        "[PR1RX] fragments=%lu frames=%lu invalid=%lu crc_fail=%lu "
        "duplicate=%lu incomplete=%lu queue_drop=%lu late=%lu "
        "underrun=%lu stream_changes=%lu queued=%u rssi=%.1f\n",
        static_cast<unsigned long>(fragments),
        static_cast<unsigned long>(frames),
        static_cast<unsigned long>(invalid),
        static_cast<unsigned long>(crc_failures),
        static_cast<unsigned long>(duplicates),
        static_cast<unsigned long>(incomplete),
        static_cast<unsigned long>(queue_drops),
        static_cast<unsigned long>(late),
        static_cast<unsigned long>(underruns),
        static_cast<unsigned long>(stream_changes),
        static_cast<unsigned>(uxQueueMessagesWaiting(audio_queue)),
        radio.getRSSI());
  }
  delay(1);
}

#endif
