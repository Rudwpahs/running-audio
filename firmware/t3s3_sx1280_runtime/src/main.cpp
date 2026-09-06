#include <Arduino.h>

#include "pr1_runtime_config.hpp"
#include "pr1_safe_telemetry.hpp"

#if PR1_RF_ENABLED
#error "Round 2 is safe-only. RF-enabled runtime is intentionally not implemented yet."
#endif

namespace {

void printBootMetadata() {
  const auto& metadata = pr1::runtime::kBootMetadata;
  const auto& pins = metadata.pins;

  Serial.println("PR1_RUNTIME_BOOT");
  Serial.printf("runtime_profile=%s\n", metadata.runtime_profile);
  Serial.printf("board_family=%s\n", metadata.board_family);
  Serial.printf("board_reference_revision=%s\n", metadata.board_reference_revision);
  Serial.printf("radio_target=%s\n", metadata.radio_target);
  Serial.printf("upstream_reference_commit=%s\n", metadata.upstream_reference_commit);
  Serial.printf("hardware_verified=%u\n", metadata.hardware_verified ? 1U : 0U);
  Serial.printf("protocol_version=%u\n", static_cast<unsigned>(metadata.protocol_version));
  Serial.printf("protocol_header_bytes=%u\n",
                static_cast<unsigned>(metadata.protocol_header_bytes));
  Serial.printf("rf_enabled=%u\n", metadata.rf_enabled ? 1U : 0U);
  Serial.printf("sx1280_cs=%d\n", pins.cs);
  Serial.printf("sx1280_rst=%d\n", pins.rst);
  Serial.printf("sx1280_sclk=%d\n", pins.sclk);
  Serial.printf("sx1280_mosi=%d\n", pins.mosi);
  Serial.printf("sx1280_miso=%d\n", pins.miso);
  Serial.printf("sx1280_dio1=%d\n", pins.dio1);
  Serial.printf("sx1280_busy=%d\n", pins.busy);
  Serial.printf("sx1280_tx_enable=%d\n", pins.tx_enable);
  Serial.printf("sx1280_rx_enable=%d\n", pins.rx_enable);
  Serial.println("PR1_RUNTIME_SAFE_IDLE");
}

void printSafeTelemetry() {
  const auto snapshot = pr1::runtime::makeSafeTelemetrySnapshot();
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

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(250);
  printBootMetadata();
  printSafeTelemetry();
}

void loop() {
  delay(1000);
}
