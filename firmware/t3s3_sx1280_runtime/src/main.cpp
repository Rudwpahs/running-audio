#include <Arduino.h>

#include "pr1_runtime_config.hpp"
#include "pr1_safe_telemetry.hpp"

#if PR1_RF_ENABLED
#include "pr1_fixed_link_runtime.hpp"
#include "pr1_sx1280_radiolib.hpp"
#endif

namespace {

void printBootMetadata() {
  const auto& metadata = pr1::runtime::kBootMetadata;
  const auto& pins = metadata.pins;

  Serial.println("PR1_RUNTIME_BOOT");
  Serial.printf("runtime_profile=%s\n", metadata.runtime_profile);
  Serial.printf("runtime_role=%s\n", pr1::runtime::runtimeRoleName());
  Serial.printf("board_family=%s\n", metadata.board_family);
  Serial.printf("board_reference_revision=%s\n", metadata.board_reference_revision);
  Serial.printf("radio_target=%s\n", metadata.radio_target);
  Serial.printf("upstream_reference_commit=%s\n", metadata.upstream_reference_commit);
  Serial.printf("hardware_verified=%u\n", metadata.hardware_verified ? 1U : 0U);
  Serial.printf("protocol_version=%u\n", static_cast<unsigned>(metadata.protocol_version));
  Serial.printf("protocol_header_bytes=%u\n",
                static_cast<unsigned>(metadata.protocol_header_bytes));
  Serial.printf("rf_enabled=%u\n", metadata.rf_enabled ? 1U : 0U);
  Serial.printf("sx1280_spi_hz=%lu\n",
                static_cast<unsigned long>(pr1::board::kSx1280SpiHz));
  Serial.printf("sx1280_cs=%d\n", pins.cs);
  Serial.printf("sx1280_rst=%d\n", pins.rst);
  Serial.printf("sx1280_sclk=%d\n", pins.sclk);
  Serial.printf("sx1280_mosi=%d\n", pins.mosi);
  Serial.printf("sx1280_miso=%d\n", pins.miso);
  Serial.printf("sx1280_dio1=%d\n", pins.dio1);
  Serial.printf("sx1280_busy=%d\n", pins.busy);
  Serial.printf("sx1280_tx_enable=%d\n", pins.tx_enable);
  Serial.printf("sx1280_rx_enable=%d\n", pins.rx_enable);
}

void printTelemetry(const pr1::telemetry::Snapshot& snapshot) {
  const std::uint32_t timestamp_us = micros();
  pr1::telemetry::forEachSnapshotField(
      snapshot, [&](pr1::telemetry::FieldValue item) {
        Serial.printf("PR1T v=%u t_us=%lu field=%s value=%lld\n",
                      static_cast<unsigned>(pr1::telemetry::kTelemetrySchemaVersion),
                      static_cast<unsigned long>(timestamp_us),
                      pr1::telemetry::fieldName(item.field),
                      static_cast<long long>(item.value));
      });
}

#if PR1_RF_ENABLED
pr1::runtime::Sx1280RadioLibPort g_radio;
pr1::runtime::FixedLinkRuntime g_runtime(
    g_radio,
    pr1::runtime::runtimeRole(),
    pr1::runtime::kFixedFlrcProfile,
    1U);
bool g_live_ready = false;

void printLiveProfile() {
  const auto& profile = pr1::runtime::kFixedFlrcProfile;
  Serial.println("PR1_FIXED_FLRC_PROFILE");
  Serial.printf("frequency_mhz=%.3f\n", static_cast<double>(profile.frequency_mhz));
  Serial.printf("bitrate_kbps=%u\n", static_cast<unsigned>(profile.bitrate_kbps));
  Serial.printf("coding_rate=%u\n", static_cast<unsigned>(profile.coding_rate));
  Serial.printf("output_dbm=%d\n", static_cast<int>(profile.output_dbm));
  Serial.printf("tx_gap_us=%lu\n", static_cast<unsigned long>(profile.tx_gap_us));
  Serial.printf("packet_bytes=%u\n", static_cast<unsigned>(pr1::kDartPacketBytes));
  Serial.println("adaptive_layers=off");
}
#endif

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(250);
  printBootMetadata();

#if PR1_RF_ENABLED
  printLiveProfile();
  g_live_ready = g_runtime.begin();
  Serial.println(g_live_ready ? "PR1_RUNTIME_LIVE_READY" : "PR1_RUNTIME_FAULT");
#else
  Serial.println("PR1_RUNTIME_SAFE_IDLE");
  printTelemetry(pr1::runtime::makeSafeTelemetrySnapshot());
#endif
}

void loop() {
#if PR1_RF_ENABLED
  if (!g_live_ready) {
    delay(1000);
    return;
  }

  g_runtime.tick(micros());

  // Serial output is intentionally pull-based in live mode. Continuous
  // telemetry printing can itself create receiver-processing stalls and would
  // contaminate the IRQ/SPI/re-arm measurements we are trying to collect.
  if (Serial.available() > 0) {
    const char command = static_cast<char>(Serial.read());
    if (command == 't' || command == 'T') {
      printTelemetry(g_runtime.metrics().snapshot());
    }
  }
#else
  delay(1000);
#endif
}
