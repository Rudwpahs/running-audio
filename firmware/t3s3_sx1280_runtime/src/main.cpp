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

void printAfhProfile();

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
  printAfhProfile();
}

// Gate B static AFH identity. The schedule fingerprint lets the host check that
// TX and RX compute the same hop sequence before any packet is sent.
void printAfhProfile() {
  using Runtime = pr1::runtime::FixedLinkRuntime;
  Serial.printf("afh_enabled=%u\n", Runtime::kAfhEnabled ? 1U : 0U);
  if constexpr (Runtime::kAfhEnabled) {
    const auto& scheduler = g_runtime.hopScheduler();
    const auto& config = scheduler.current();
    Serial.printf("afh_session_id=%u\n", static_cast<unsigned>(config.session_id));
    Serial.printf("afh_session_seed=0x%016llx\n",
                  static_cast<unsigned long long>(config.session_seed));
    Serial.printf("afh_map_version=%u\n", static_cast<unsigned>(config.map_version));
    Serial.printf("afh_active_channels=%u\n", static_cast<unsigned>(config.map.activeCount()));
    Serial.printf("afh_resync_after_misses=%u\n",
                  static_cast<unsigned>(PR1_AFH_RESYNC_AFTER_MISSES));
    Serial.printf("afh_rendezvous_first=%u\n",
                  static_cast<unsigned>(scheduler.rendezvousChannel(0)));
    Serial.print("afh_schedule_fp=");
    for (unsigned i = 0; i < 48; ++i) {
      Serial.printf(i == 0 ? "%u" : ",%u", static_cast<unsigned>(scheduler.channelForSequence(i)));
    }
    Serial.println();
  }
}

void printHop() {
  using Runtime = pr1::runtime::FixedLinkRuntime;
  if constexpr (Runtime::kAfhEnabled) {
    const auto& h = g_runtime.hopTelemetry();
    const auto& config = g_runtime.hopScheduler().current();
    Serial.printf(
        "PR1H t_us=%lu session_id=%u map_version=%u locked=%u channel=%u logical=%llu "
        "retunes=%lu retune_failures=%lu standby_failures=%lu timeout_advances=%lu "
        "resync_entries=%lu locks=%lu agree=%lu disagree=%lu period_est_us=%lu "
        "max_consecutive_timeouts=%lu retune_us_p99=%lu retune_us_max=%lu "
        "hop_compute_us_p99=%lu hop_compute_us_max=%lu\n",
        static_cast<unsigned long>(micros()), static_cast<unsigned>(config.session_id),
        static_cast<unsigned>(config.map_version), h.locked ? 1U : 0U,
        static_cast<unsigned>(h.current_channel), static_cast<unsigned long long>(h.logical),
        static_cast<unsigned long>(h.retunes), static_cast<unsigned long>(h.retune_failures),
        static_cast<unsigned long>(h.standby_failures),
        static_cast<unsigned long>(h.timeout_advances),
        static_cast<unsigned long>(h.resync_entries), static_cast<unsigned long>(h.locks),
        static_cast<unsigned long>(h.schedule_agree),
        static_cast<unsigned long>(h.schedule_disagree),
        static_cast<unsigned long>(h.period_est_us),
        static_cast<unsigned long>(h.max_consecutive_timeouts),
        static_cast<unsigned long>(h.retune_us.percentile(99)),
        static_cast<unsigned long>(h.retune_us.maxUs()),
        static_cast<unsigned long>(h.hop_compute_us.percentile(99)),
        static_cast<unsigned long>(h.hop_compute_us.maxUs()));
    Serial.print("PR1HC ok=");
    for (unsigned c = 0; c < h.channel_ok.size(); ++c) {
      Serial.printf(c == 0 ? "%lu" : ",%lu", static_cast<unsigned long>(h.channel_ok[c]));
    }
    Serial.print(" crc=");
    for (unsigned c = 0; c < h.channel_crc.size(); ++c) {
      Serial.printf(c == 0 ? "%lu" : ",%lu", static_cast<unsigned long>(h.channel_crc[c]));
    }
    Serial.println();
  } else {
    Serial.println("PR1H afh_enabled=0");
  }
}

// Last 256 hop events (oldest first). Pulled only at the end of a run.
void dumpHopRing() {
  using Runtime = pr1::runtime::FixedLinkRuntime;
  if constexpr (Runtime::kAfhEnabled) {
    const auto& h = g_runtime.hopTelemetry();
    const std::uint32_t total = h.ring_written;
    const std::uint32_t count = total < h.ring.size() ? total : h.ring.size();
    Serial.printf("PR1HE_BEGIN total=%lu count=%lu\n", static_cast<unsigned long>(total),
                  static_cast<unsigned long>(count));
    for (std::uint32_t i = total - count; i < total; ++i) {
      const auto& e = h.ring[i % h.ring.size()];
      Serial.printf("PR1HE t_us=%lu kind=%u logical=%lu seq=%u ch=%u rssi=%d\n",
                    static_cast<unsigned long>(e.t_us), static_cast<unsigned>(e.kind),
                    static_cast<unsigned long>(e.logical_lo), static_cast<unsigned>(e.raw_sequence),
                    static_cast<unsigned>(e.channel), static_cast<int>(e.rssi_dbm));
    }
    const std::uint32_t kept = h.anomalies_written < h.anomalies.size()
                                   ? h.anomalies_written
                                   : static_cast<std::uint32_t>(h.anomalies.size());
    Serial.printf("PR1HA_BEGIN total=%lu kept=%lu\n", static_cast<unsigned long>(h.anomalies_written),
                  static_cast<unsigned long>(kept));
    for (std::uint32_t i = 0; i < kept; ++i) {
      const auto& e = h.anomalies[i];
      Serial.printf("PR1HA t_us=%lu kind=%u logical=%lu seq=%u ch=%u rssi=%d\n",
                    static_cast<unsigned long>(e.t_us), static_cast<unsigned>(e.kind),
                    static_cast<unsigned long>(e.logical_lo), static_cast<unsigned>(e.raw_sequence),
                    static_cast<unsigned>(e.channel), static_cast<int>(e.rssi_dbm));
    }
    Serial.println("PR1HE_END");
  }
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
    } else if (command == 'h') {
      printHop();
    } else if (command == 'H') {
      dumpHopRing();
    }
  }
#else
  delay(1000);
#endif
}
