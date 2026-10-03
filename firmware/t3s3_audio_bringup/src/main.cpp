// PR1 audio prototype, step D2-1: RF-free I2S -> MAX98357A bring-up.
//
// Reused (not redesigned) from the earlier prototypes:
//   * I2S setup: origin/agent/pr1-h59401-prototype-v3 @ d14845b src/rx_role.cpp configureI2s()
//     (I2S_NUM_1 master TX, 16-bit, RIGHT_LEFT duplicated mono, STAND_I2S, 8 DMA buffers,
//     no APLL, SD/amp-enable driven HIGH).
//   * Pins BCLK 40 / LRCLK 41 / DOUT 39 / SD 38: d14845b include/pr1_config.h and the
//     LilyGO T3-S3 MVSR onboard MAX98357A (docs/HARDWARE_ROLE_MATRIX.md).
//   * 32 kHz rate: 2026-08-23 checkpoint PR1_Audio_Stream.ino.
// Audio runs in a task pinned to core 0, as in the August sketch (core 1 is the radio core
// in the RF builds; nothing radio-related runs here).

#include <Arduino.h>
#include <driver/i2s.h>

#include <cmath>
#include <cstdint>

namespace {

constexpr int kPinBclk = 40;
constexpr int kPinLrclk = 41;
constexpr int kPinDout = 39;
constexpr int kPinAmpSd = 38;  // MAX98357A SD_MODE: HIGH = on, LOW = shutdown
constexpr i2s_port_t kPort = I2S_NUM_1;

#ifndef PR1_AUDIO_SAMPLE_RATE
#define PR1_AUDIO_SAMPLE_RATE 32000
#endif
#ifndef PR1_TONE_HZ
#define PR1_TONE_HZ 440
#endif
#ifndef PR1_TONE_AMPLITUDE
#define PR1_TONE_AMPLITUDE 4096  // PR1_MAX_ABS_SAMPLE in the July prototype
#endif
constexpr std::uint32_t kRate = PR1_AUDIO_SAMPLE_RATE;
constexpr std::size_t kChunkFrames = 160;  // stereo frames per i2s_write (5 ms at 32 kHz)

struct Stats {
  volatile std::uint32_t frames_written = 0;
  volatile std::uint32_t short_writes = 0;  // i2s_write returned fewer bytes than asked
  volatile std::uint32_t write_errors = 0;
  volatile std::uint32_t max_write_us = 0;
  volatile bool playing = true;
  volatile int core = -1;
} g_stats;

esp_err_t g_install = ESP_FAIL, g_pins = ESP_FAIL, g_zero = ESP_FAIL;

void configureI2s() {
  pinMode(kPinAmpSd, OUTPUT);
  digitalWrite(kPinAmpSd, LOW);  // keep the amp off until DMA buffers are zeroed (no pop)

  i2s_config_t config = {};
  config.mode = static_cast<i2s_mode_t>(I2S_MODE_MASTER | I2S_MODE_TX);
  config.sample_rate = kRate;
  config.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT;
  config.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT;
  config.communication_format = I2S_COMM_FORMAT_STAND_I2S;
  config.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1;
  config.dma_buf_count = 8;
  config.dma_buf_len = kChunkFrames;
  config.use_apll = false;
  config.tx_desc_auto_clear = true;  // underrun -> silence, not a repeated buffer
  config.fixed_mclk = 0;

  i2s_pin_config_t pins = {};
  pins.mck_io_num = I2S_PIN_NO_CHANGE;
  pins.bck_io_num = kPinBclk;
  pins.ws_io_num = kPinLrclk;
  pins.data_out_num = kPinDout;
  pins.data_in_num = I2S_PIN_NO_CHANGE;

  g_install = i2s_driver_install(kPort, &config, 0, nullptr);
  g_pins = i2s_set_pin(kPort, &pins);
  g_zero = i2s_zero_dma_buffer(kPort);
  digitalWrite(kPinAmpSd, HIGH);
}

void audioTask(void*) {
  g_stats.core = xPortGetCoreID();
  configureI2s();  // I2S interrupt is allocated on this core (core 0)
  // One period-exact table would need kRate % freq == 0; use a phase accumulator instead.
  static std::int16_t sine[256];
  for (int i = 0; i < 256; ++i) {
    sine[i] = static_cast<std::int16_t>(PR1_TONE_AMPLITUDE * std::sin(2.0 * M_PI * i / 256.0));
  }
  const std::uint32_t step = static_cast<std::uint32_t>((static_cast<std::uint64_t>(PR1_TONE_HZ) << 32) / kRate);
  std::uint32_t phase = 0;
  static std::int16_t buf[kChunkFrames * 2];
  for (;;) {
    for (std::size_t i = 0; i < kChunkFrames; ++i) {
      const std::int16_t s = g_stats.playing ? sine[phase >> 24] : 0;
      phase += step;
      buf[2 * i] = s;      // right
      buf[2 * i + 1] = s;  // left (MAX98357A plays L, R or (L+R)/2 depending on SD level)
    }
    size_t written = 0;
    const std::uint32_t t0 = micros();
    const esp_err_t err = i2s_write(kPort, buf, sizeof(buf), &written, pdMS_TO_TICKS(100));
    const std::uint32_t dt = micros() - t0;
    if (dt > g_stats.max_write_us) g_stats.max_write_us = dt;
    if (err != ESP_OK) ++g_stats.write_errors;
    if (written != sizeof(buf)) ++g_stats.short_writes;
    g_stats.frames_written += written / 4U;
  }
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println("PR1_AUDIO_BRINGUP");
  Serial.printf("audio_mode=tone tone_hz=%u amplitude=%u sample_rate=%lu bits=16 channels=RIGHT_LEFT(mono dup) "
                "port=I2S1 bclk=%d lrclk=%d dout=%d sd=%d dma=8x%u apll=0 rf=off\n",
                PR1_TONE_HZ, PR1_TONE_AMPLITUDE, static_cast<unsigned long>(kRate), kPinBclk, kPinLrclk, kPinDout,
                kPinAmpSd, static_cast<unsigned>(kChunkFrames));
  xTaskCreatePinnedToCore(audioTask, "pr1_audio", 4096, nullptr, 5, nullptr, 0);
  delay(200);
  Serial.printf("[AUDIO] MAX98357A I2S1 install=%s set_pin=%s zero=%s core=%d\n", esp_err_to_name(g_install),
                esp_err_to_name(g_pins), esp_err_to_name(g_zero), g_stats.core);
  Serial.println("commands: m = mute/unmute, s = stats");
}

void loop() {
  static std::uint32_t last_ms = 0;
  if (Serial.available() > 0) {
    const char c = static_cast<char>(Serial.read());
    if (c == 'm') {
      g_stats.playing = !g_stats.playing;
      Serial.printf("[AUDIO] %s\n", g_stats.playing ? "tone on" : "muted (zeros still streamed)");
    }
  }
  if (millis() - last_ms >= 2000U) {
    last_ms = millis();
    Serial.printf("[AUDIO] t_ms=%lu frames=%lu expected=%lu short_writes=%lu errors=%lu max_write_us=%lu playing=%u\n",
                  static_cast<unsigned long>(last_ms), static_cast<unsigned long>(g_stats.frames_written),
                  static_cast<unsigned long>(static_cast<std::uint64_t>(last_ms) * kRate / 1000U),
                  static_cast<unsigned long>(g_stats.short_writes), static_cast<unsigned long>(g_stats.write_errors),
                  static_cast<unsigned long>(g_stats.max_write_us), g_stats.playing ? 1U : 0U);
  }
  delay(5);
}
