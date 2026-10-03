#include <Arduino.h>

#include <cstring>

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
void printQuality();

void printLiveProfile() {
  const auto& profile = pr1::runtime::kFixedFlrcProfile;
  Serial.println("PR1_FIXED_FLRC_PROFILE");
  Serial.printf("frequency_mhz=%.3f\n", static_cast<double>(profile.frequency_mhz));
  Serial.printf("bitrate_kbps=%u\n", static_cast<unsigned>(profile.bitrate_kbps));
  Serial.printf("coding_rate=%u\n", static_cast<unsigned>(profile.coding_rate));
  Serial.printf("output_dbm=%d\n", static_cast<int>(profile.output_dbm));
  Serial.printf("tx_gap_us=%lu\n", static_cast<unsigned long>(profile.tx_gap_us));
  Serial.printf("packet_bytes=%u\n", static_cast<unsigned>(pr1::kDartPacketBytes));
  // Gate C turns on exactly one adaptive layer: the AFH channel map.
  Serial.println(pr1::runtime::FixedLinkRuntime::kAdaptive ? "adaptive_layers=channel_map"
                                                           : "adaptive_layers=off");
  printAfhProfile();
}

// Non-blocking: if the USB CDC buffer cannot take the whole line, drop it (the
// runtime counts drops) instead of stalling the radio loop.
bool emitLine(const char* line) {
  const int needed = static_cast<int>(std::strlen(line)) + 2;
  if (Serial.availableForWrite() < needed) return false;
  Serial.println(line);
  return true;
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
    Serial.printf("adaptive_map=%u\n", Runtime::kAdaptive ? 1U : 0U);
    if constexpr (Runtime::kAdaptive) {
      const auto& q = g_runtime.estimator().config();
      Serial.printf("map_lead_frames=%u\n", static_cast<unsigned>(PR1_MAP_LEAD_FRAMES));
      Serial.printf("map_guard_frames=%u\n", static_cast<unsigned>(PR1_MAP_GUARD_FRAMES));
      Serial.printf(
          "quality_cfg=fast_shift:%u,slow_shift:%u,suspect_q15:%u,exclude_q15:%u,recover_q15:%u,"
          "suspect_losses:%u,exclude_losses:%u,recover_successes:%u,probe_ms:%lu-%lu,min_active:%u\n",
          q.alpha_fast_shift, q.alpha_slow_shift, q.suspect_pdr_q15, q.exclude_pdr_q15,
          q.recover_pdr_q15, q.suspect_losses, q.exclude_losses, q.recover_successes,
          static_cast<unsigned long>(q.initial_probe_ms), static_cast<unsigned long>(q.max_probe_ms),
          q.minimum_active_channels);
    }
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
    Serial.printf("PR1HQ acq_anchors=%lu acq_crc=%lu acq_frames=%u acq_done_us=%lu\n",
                  static_cast<unsigned long>(h.acq_anchors), static_cast<unsigned long>(h.acq_crc),
                  static_cast<unsigned>(h.acq_frames), static_cast<unsigned long>(h.acq_done_us));
    Serial.print("PR1HC ok=");
    for (unsigned c = 0; c < h.channel_ok.size(); ++c) {
      Serial.printf(c == 0 ? "%lu" : ",%lu", static_cast<unsigned long>(h.channel_ok[c]));
    }
    Serial.print(" crc=");
    for (unsigned c = 0; c < h.channel_crc.size(); ++c) {
      Serial.printf(c == 0 ? "%lu" : ",%lu", static_cast<unsigned long>(h.channel_crc[c]));
    }
    Serial.println();
    if constexpr (Runtime::kAdaptive) printQuality();
  } else {
    Serial.println("PR1H afh_enabled=0");
  }
}

// Gate C dump (end of run only): summary, per-channel estimator state, map/probe events.
void printQuality() {
  using Runtime = pr1::runtime::FixedLinkRuntime;
  if constexpr (Runtime::kAdaptive) {
    const auto& m = g_runtime.mapTelemetry();
    const auto& est = g_runtime.estimator();
    const auto& cur = g_runtime.hopScheduler().current();
    const auto& pend = g_runtime.hopScheduler().pending();
    Serial.printf(
        "PR1QS map_version=%u active=%u bits=%010llx pending=%u pending_v=%u pending_act=%llu "
        "min_active_seen=%u proposals=%lu commits=%lu commit_late=%lu expired=%lu activations=%lu "
        "tx_rejects=%lu outcomes_dropped=%lu events=%lu emit_dropped=%lu activation_count=%lu\n",
        static_cast<unsigned>(cur.map_version), static_cast<unsigned>(cur.map.activeCount()),
        static_cast<unsigned long long>(cur.map.bits), pend.valid ? 1U : 0U,
        static_cast<unsigned>(pend.map_version), static_cast<unsigned long long>(pend.activation_sequence),
        static_cast<unsigned>(m.min_active_seen), static_cast<unsigned long>(m.proposals),
        static_cast<unsigned long>(m.commits), static_cast<unsigned long>(m.commit_late),
        static_cast<unsigned long>(m.expired), static_cast<unsigned long>(m.activations),
        static_cast<unsigned long>(m.tx_rejects), static_cast<unsigned long>(m.outcomes_dropped),
        static_cast<unsigned long>(m.events_written), static_cast<unsigned long>(m.emit_dropped),
        static_cast<unsigned long>(m.activation_count));
    for (std::uint32_t i = 0; i < m.activation_count && i < m.activation_log.size(); ++i) {
      const auto& a = m.activation_log[i];
      Serial.printf("PR1QA v=%u activation=%lu applied_at=%lu\n", static_cast<unsigned>(a.version),
                    static_cast<unsigned long>(a.activation_lo), static_cast<unsigned long>(a.applied_at_lo));
    }
    for (unsigned c = 0; c < pr1::afh::kChannelCount; ++c) {
      const auto& s = est.channel(static_cast<std::uint8_t>(c));
      const auto& k = m.channels[c];
      Serial.printf(
          "PR1QC ch=%u mhz=%lu state=%u fast_q15=%u slow_q15=%u ok=%lu crc=%lu timeout=%lu "
          "probe_ok=%lu probe_fail=%lu suspects=%u exclusions=%u reinclusions=%u last_excl_ms=%lu\n",
          c, static_cast<unsigned long>(pr1::afh::frequencyHz(static_cast<std::uint8_t>(c)) / 1000000UL),
          static_cast<unsigned>(s.state), s.pdr_fast_q15, s.pdr_slow_q15,
          static_cast<unsigned long>(k.ok), static_cast<unsigned long>(k.crc),
          static_cast<unsigned long>(k.timeout), static_cast<unsigned long>(k.probe_ok),
          static_cast<unsigned long>(k.probe_fail), k.suspects, k.exclusions, k.reinclusions,
          static_cast<unsigned long>(k.last_excluded_ms));
    }
    const std::uint32_t kept = m.events_written < m.events.size()
                                   ? m.events_written
                                   : static_cast<std::uint32_t>(m.events.size());
    for (std::uint32_t i = 0; i < kept; ++i) {
      const auto& e = m.events[i];
      Serial.printf("PR1QE t_ms=%lu kind=%u logical=%lu target=%lu id=%u v=%u ch=%u value=%u bits=%010llx\n",
                    static_cast<unsigned long>(e.t_ms), static_cast<unsigned>(e.kind),
                    static_cast<unsigned long>(e.logical_lo), static_cast<unsigned long>(e.target_lo),
                    static_cast<unsigned>(e.id), static_cast<unsigned>(e.version),
                    static_cast<unsigned>(e.channel), static_cast<unsigned>(e.value),
                    static_cast<unsigned long long>(e.bits));
    }
    Serial.println("PR1QE_END");
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
  g_runtime.setLineSink(&emitLine);
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
  // Single-char pulls (t/h/H) are handled immediately; anything else is a
  // newline-terminated control line for the Gate C bench control plane.
  static char line[160];
  static unsigned line_len = 0;
  if (Serial.available() > 0) {
    const char command = static_cast<char>(Serial.read());
    if (line_len == 0 && (command == 't' || command == 'T')) {
      printTelemetry(g_runtime.metrics().snapshot());
    } else if (line_len == 0 && command == 'h') {
      printHop();
    } else if (line_len == 0 && command == 'H') {
      dumpHopRing();
    } else if (pr1::runtime::FixedLinkRuntime::kAdaptive) {
      // Control lines exist only in Gate C builds; Gate B ignores stray bytes as before.
      if (command == '\n' || command == '\r') {
        if (line_len > 0) {
          line[line_len] = '\0';
          g_runtime.onControlLine(line);
          line_len = 0;
        }
      } else if (line_len + 1 < sizeof(line)) {
        line[line_len++] = command;
      }
    }
  }
#else
  delay(1000);
#endif
}
