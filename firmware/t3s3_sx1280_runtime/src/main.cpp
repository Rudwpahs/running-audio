#include <Arduino.h>

#include <atomic>
#include <cstring>

#include "pr1_runtime_config.hpp"
#include "pr1_safe_telemetry.hpp"

#if PR1_RF_ENABLED
#include "esp_rom_sys.h"
#include "esp_timer.h"
#include "hal/usb_serial_jtag_ll.h"
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

// Gate C1 control plane (core 0). The USB-Serial/JTAG interrupt is allocated on the
// core that calls Serial.begin(), so this task owns begin() and all control-line
// I/O; the radio loop on core 1 only touches the record queues and the pull flag.
namespace cp = pr1::runtime::ctrl;
cp::Spsc<cp::In, 16> g_ctrl_in;    // core 0 -> core 1
cp::Spsc<cp::Out, 32> g_ctrl_out;  // core 1 -> core 0
std::atomic<std::uint32_t> g_pull{0};
std::atomic<bool> g_cp_ready{false};
std::atomic<bool> g_dumping{false};  // radio loop is printing a pull reply: hold control output
std::atomic<bool> g_poll_mode{false};
std::atomic<bool> g_dump_ready{false};  // control core released the IN FIFO to HWCDC  // boot banner done: the control core owns the USB FIFOs
struct ControlPlaneStats {
  volatile int core = -1;
  volatile std::uint32_t lines = 0;
  volatile std::uint32_t parse_fail = 0;
  volatile std::uint32_t in_dropped = 0;
  volatile std::uint32_t out_lines = 0;
  volatile std::uint32_t write_waits = 0;
  volatile std::uint32_t stack_free = 0;
  volatile std::uint32_t window_waits = 0;  // control-core passes skipped (RX window closed)
  volatile std::uint32_t work_us_max = 0;   // longest USB work burst
} g_cp;

PR1_IRAM bool pushControlOut(const cp::Out& out) { return g_ctrl_out.push(out); }

// HWCDC::flush() waits forever if no host is reading; drain for at most `ms` instead.
void drainSerial(std::uint32_t ms) {
  // Done when the free space stops changing for 5 ms (drained, or nobody reading).
  const std::uint32_t t0 = millis();
  int last = -1;
  int stable = 0;
  do {
    delay(1);
    const int room = Serial.availableForWrite();
    if (room == last) {
      if (++stable >= 5) break;
    } else {
      stable = 0;
      last = room;
    }
  } while (millis() - t0 < ms);
}
PR1_IRAM bool popControlIn(cp::In* in) { return g_ctrl_in.pop(in); }

// Live-run USB I/O: polled FIFO access from IRAM (no HWCDC driver, no USB interrupt).
// HWCDC (flash code, interrupt driven) is used only for the boot banner and the
// post-run pull dumps; during the run its IN/OUT interrupts stay masked, so the host's
// OUT data waits in the 64-byte hardware FIFO (host NAKed) until the control core reads it.
constexpr std::uint32_t kUsbCtrlIntr =
    USB_SERIAL_JTAG_INTR_SERIAL_IN_EMPTY | USB_SERIAL_JTAG_INTR_SERIAL_OUT_RECV_PKT;

struct UsbLineState {
  char line[160];
  unsigned line_len = 0;
  char text[224];
  int text_len = 0;  // formatted line being written
  int text_off = 0;  // bytes of it already in the IN FIFO
};

// Input: drain the OUT FIFO into lines, parse, queue. Pull chars are flagged.
PR1_IRAM void pollUsbInput(UsbLineState& st) {
  std::uint8_t buf[64];
  const std::uint32_t n = usb_serial_jtag_ll_read_rxfifo(buf, sizeof(buf));
  for (std::uint32_t i = 0; i < n; ++i) {
    const char c = static_cast<char>(buf[i]);
    // Single-char pulls (t/h/H) are served by the radio loop after the run.
    if (st.line_len == 0 && (c == 't' || c == 'T' || c == 'h' || c == 'H')) {
      g_pull.store(static_cast<std::uint32_t>(c));
    } else if (c == '\n' || c == '\r') {
      if (st.line_len > 0) {
        st.line[st.line_len] = '\0';
        st.line_len = 0;
        ++g_cp.lines;
        cp::In m{};
        if (pr1::runtime::FixedLinkRuntime::kAdaptive && cp::parse(st.line, &m)) {
          if (!g_ctrl_in.push(m)) ++g_cp.in_dropped;
        } else {
          ++g_cp.parse_fail;
        }
      }
    } else if (pr1::runtime::FixedLinkRuntime::kAdaptive && st.line_len + 1 < sizeof(st.line)) {
      st.line[st.line_len++] = c;
    }
  }
}

// Output: format the next record and push it into the IN FIFO (64 bytes per chunk).
PR1_IRAM void pollUsbOutput(UsbLineState& st) {
  if (st.text_len == 0) {
    cp::Out o{};
    while (st.text_len == 0 && g_ctrl_out.pop(&o)) {
      const int n = cp::format(o, st.text, sizeof(st.text) - 2U);
      if (n <= 0) continue;
      st.text[n] = '\r';
      st.text[n + 1] = '\n';
      st.text_len = n + 2;
      st.text_off = 0;
    }
    if (st.text_len == 0) return;
  }
  if (!usb_serial_jtag_ll_txfifo_writable()) {
    ++g_cp.write_waits;
    return;
  }
  const std::uint32_t w = usb_serial_jtag_ll_write_txfifo(
      reinterpret_cast<const std::uint8_t*>(st.text + st.text_off), static_cast<std::uint32_t>(st.text_len - st.text_off));
  usb_serial_jtag_ll_txfifo_flush();
  st.text_off += static_cast<int>(w);
  if (st.text_off >= st.text_len) {
    st.text_len = 0;
    st.text_off = 0;
    ++g_cp.out_lines;
  }
}

PR1_IRAM void controlLoop(bool gated_role) {
  UsbLineState st{};
  bool dump_mode = false;
  std::uint32_t loops = 0;
  for (;;) {
    if (g_dumping.load()) {
      if (st.text_len != 0) {
        pollUsbOutput(st);  // radio loop is idle-waiting: finish the line, no window
        esp_rom_delay_us(20);
        continue;
      }
      // Radio loop prints through HWCDC: let its IN interrupt run; keep reading pulls.
      if (!dump_mode) {
        usb_serial_jtag_ll_ena_intr_mask(USB_SERIAL_JTAG_INTR_SERIAL_IN_EMPTY);
        dump_mode = true;
        g_dump_ready.store(true);
      }
      pollUsbInput(st);
      vTaskDelay(1);
      continue;
    }
    if (dump_mode) {
      // Let HWCDC finish sending what is still in its ring (post-run only), then take
      // the FIFO back. Host bytes meanwhile wait in the OUT FIFO (OUT interrupt masked).
      vTaskDelay(300);
      usb_serial_jtag_ll_disable_intr_mask(USB_SERIAL_JTAG_INTR_SERIAL_IN_EMPTY);
      dump_mode = false;
      g_dump_ready.store(false);
    }
    // RX: touch USB only inside the window the radio core publishes after re-arm.
    if (!gated_role || g_runtime.controlWindowOpen()) {
      const std::uint32_t t0 = static_cast<std::uint32_t>(esp_timer_get_time());
      pollUsbInput(st);
      pollUsbOutput(st);
      const std::uint32_t work_us = static_cast<std::uint32_t>(esp_timer_get_time()) - t0;
      if (work_us > g_cp.work_us_max) g_cp.work_us_max = work_us;
    } else {
      ++g_cp.window_waits;
    }
    if (!gated_role) {
      vTaskDelay(1);
      continue;
    }
    if (++loops >= 2000U) {
      loops = 0;
      g_cp.stack_free = uxTaskGetStackHighWaterMark(nullptr);
      vTaskDelay(1);  // keep IDLE0 / the task watchdog fed
      continue;
    }
    esp_rom_delay_us(20);  // CPU cycle-counter wait (ROM): no flash, timer or bus traffic
  }
}

void controlTask(void*) {
  Serial.begin(115200);
  g_cp.core = xPortGetCoreID();
  g_cp_ready.store(true);
  const bool gated_role = pr1::runtime::runtimeRole() == pr1::runtime::RuntimeRole::Rx;
  while (!g_poll_mode.load()) vTaskDelay(1);  // boot banner goes through HWCDC
  vTaskDelay(300);                            // let HWCDC finish sending it
  usb_serial_jtag_ll_disable_intr_mask(kUsbCtrlIntr | USB_SERIAL_JTAG_INTR_BUS_RESET);
  g_cp.stack_free = uxTaskGetStackHighWaterMark(nullptr);
  controlLoop(gated_role);
}

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
  Serial.printf("ctrl_plane_core=%d\n", static_cast<int>(g_cp.core));
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
    Serial.printf("adaptive_map=%u\n", Runtime::kAdaptive ? 1U : 0U);
    if constexpr (Runtime::kAdaptive) {
      const auto& q = g_runtime.estimator().config();
      Serial.printf("c3_cfg=strike_shift:%u,strike_base_ms:%lu,strike_cap_ms:%lu,strike_decay_ms:%lu,"
                    "neighbor_radius:%u,neighbor_min_bad:%u,neighbor_bad_slow_q15:%u,neighbor_direct_fast_q15:%u,"
                    "probe_window:%u,probation:%u\n",
                    q.strike_backoff_max_shift, static_cast<unsigned long>(q.strike_probe_ms),
                    static_cast<unsigned long>(q.strike_max_probe_ms), static_cast<unsigned long>(q.strike_decay_ms),
                    q.neighbor_radius, q.neighbor_min_bad, q.neighbor_bad_slow_q15, q.neighbor_direct_fast_q15,
                    q.reinstate_probe_window, q.probation_visits);
      Serial.printf("map_lead_frames=%u\n", static_cast<unsigned>(PR1_MAP_LEAD_FRAMES));
      Serial.printf("map_guard_frames=%u\n", static_cast<unsigned>(PR1_MAP_GUARD_FRAMES));
      Serial.printf(
          "quality_cfg=fast_shift:%u,slow_shift:%u,suspect_q15:%u,exclude_q15:%u,recover_q15:%u,"
          "suspect_losses:%u,exclude_losses:%u,recover_successes:%u,probe_ms:%lu-%lu,min_active:%u,"
          "exclude_slow_q15:%u,reinstate_successes:%u,map_min_interval_ms:%u\n",
          q.alpha_fast_shift, q.alpha_slow_shift, q.suspect_pdr_q15, q.exclude_pdr_q15,
          q.recover_pdr_q15, q.suspect_losses, q.exclude_losses, q.recover_successes,
          static_cast<unsigned long>(q.initial_probe_ms), static_cast<unsigned long>(q.max_probe_ms),
          q.minimum_active_channels, q.exclude_slow_pdr_q15, q.reinstate_probe_successes,
          static_cast<unsigned>(PR1_MAP_MIN_INTERVAL_MS));
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
    Serial.printf("PR1CP core=%d lines=%lu parse_fail=%lu in_dropped=%lu out_lines=%lu write_waits=%lu "
                  "stack_free=%lu window_waits=%lu work_us_max=%lu\n",
                  static_cast<int>(g_cp.core), static_cast<unsigned long>(g_cp.lines),
                  static_cast<unsigned long>(g_cp.parse_fail), static_cast<unsigned long>(g_cp.in_dropped),
                  static_cast<unsigned long>(g_cp.out_lines), static_cast<unsigned long>(g_cp.write_waits),
                  static_cast<unsigned long>(g_cp.stack_free), static_cast<unsigned long>(g_cp.window_waits),
                  static_cast<unsigned long>(g_cp.work_us_max));
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
        "tx_rejects=%lu outcomes_dropped=%lu events=%lu emit_dropped=%lu activation_count=%lu ctrl_us_p99=%lu ctrl_us_max=%lu ctrl_us_n=%u ctrl_us_p50=%lu ctrl_us_p95=%lu\n",
        static_cast<unsigned>(cur.map_version), static_cast<unsigned>(cur.map.activeCount()),
        static_cast<unsigned long long>(cur.map.bits), pend.valid ? 1U : 0U,
        static_cast<unsigned>(pend.map_version), static_cast<unsigned long long>(pend.activation_sequence),
        static_cast<unsigned>(m.min_active_seen), static_cast<unsigned long>(m.proposals),
        static_cast<unsigned long>(m.commits), static_cast<unsigned long>(m.commit_late),
        static_cast<unsigned long>(m.expired), static_cast<unsigned long>(m.activations),
        static_cast<unsigned long>(m.tx_rejects), static_cast<unsigned long>(m.outcomes_dropped),
        static_cast<unsigned long>(m.events_written), static_cast<unsigned long>(m.emit_dropped),
        static_cast<unsigned long>(m.activation_count),
        static_cast<unsigned long>(m.ctrl_us.percentile(99)), static_cast<unsigned long>(m.ctrl_us.maxUs()),
        static_cast<unsigned>(m.ctrl_us.size()), static_cast<unsigned long>(m.ctrl_us.percentile(50)),
        static_cast<unsigned long>(m.ctrl_us.percentile(95)));
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
#if PR1_RF_ENABLED
  // Serial.begin() runs inside the core-0 task so the USB interrupt lands on core 0.
  xTaskCreatePinnedToCore(controlTask, "pr1_ctrl", 4096, nullptr, 2, nullptr, 0);
  while (!g_cp_ready.load()) delay(1);
#else
  Serial.begin(115200);
#endif
  delay(250);
  printBootMetadata();

#if PR1_RF_ENABLED
  printLiveProfile();
  g_runtime.setControlPlane(&pushControlOut, &popControlIn);
  g_live_ready = g_runtime.begin();
  Serial.println(g_live_ready ? "PR1_RUNTIME_LIVE_READY" : "PR1_RUNTIME_FAULT");
  drainSerial(300);
  g_poll_mode.store(true);
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
  // The core-0 control task reads the port; here only its pull flag is checked.
  if (g_pull.load(std::memory_order_relaxed) != 0U) {
    const char command = static_cast<char>(g_pull.exchange(0U));
    g_dumping.store(true);
    // Wait (bounded) until the control core has finished any line in flight.
    for (int i = 0; i < 50 && !g_dump_ready.load(); ++i) vTaskDelay(1);
    if (command == 't' || command == 'T') {
      printTelemetry(g_runtime.metrics().snapshot());
    } else if (command == 'h') {
      printHop();
    } else if (command == 'H') {
      dumpHopRing();
    }
    drainSerial(300);
    g_dumping.store(false);
  }
#else
  delay(1000);
#endif
}
