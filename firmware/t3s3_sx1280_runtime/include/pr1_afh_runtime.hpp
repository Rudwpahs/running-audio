#pragma once

// Gate B (issue #52): static all-channel AFH wiring for the fixed-link runtime.
// Hop bookkeeping lives here so the fixed-channel baseline path (PR1_ENABLE_AFH=0)
// compiles to the unchanged code and the PR1T telemetry schema is not touched.

#include <array>
#include <cstdint>

#include "../../common/pr1_afh.hpp"
#include "../../common/pr1_instrumentation.hpp"

#ifndef PR1_AFH_SESSION_SEED
#define PR1_AFH_SESSION_SEED 0x5052314441525431ULL  // "PR1DART1"
#endif

#ifndef PR1_AFH_SESSION_ID
#define PR1_AFH_SESSION_ID 1
#endif

#ifndef PR1_AFH_MAP_VERSION
#define PR1_AFH_MAP_VERSION 1
#endif

// RX: consecutive timeout advances before giving up the predicted schedule and
// parking on a rendezvous channel until any packet is heard again.
#ifndef PR1_AFH_RESYNC_AFTER_MISSES
#define PR1_AFH_RESYNC_AFTER_MISSES 8
#endif

// RX: timeout used until two consecutive packets give a period estimate.
#ifndef PR1_AFH_INITIAL_TIMEOUT_US
#define PR1_AFH_INITIAL_TIMEOUT_US 20000
#endif

// Diagnostic: TX busy-wait after the frequency write, before transmit (us).
#ifndef PR1_AFH_TX_SETTLE_US
#define PR1_AFH_TX_SETTLE_US 0
#endif

// Diagnostic: hop logically but always program this channel index (-1 = off).
#ifndef PR1_AFH_DIAG_FIXED_CHANNEL
#define PR1_AFH_DIAG_FIXED_CHANNEL -1
#endif
#if PR1_AFH_DIAG_FIXED_CHANNEL >= 40
#error "PR1_AFH_DIAG_FIXED_CHANNEL must be -1 or 0..39"
#endif

// Diagnostic: hop logically but always program 2404 MHz (see retune()).
#ifndef PR1_AFH_DIAG_SAME_FREQ
#define PR1_AFH_DIAG_SAME_FREQ 0
#endif

// RX: margin after the predicted RX-done before a frame is declared lost.
#ifndef PR1_AFH_LOSS_MARGIN_MIN_US
#define PR1_AFH_LOSS_MARGIN_MIN_US 150
#endif
#ifndef PR1_AFH_LOSS_MARGIN_MAX_US
#define PR1_AFH_LOSS_MARGIN_MAX_US 400
#endif

namespace pr1::runtime::afhrt {

inline afh::ScheduleConfig staticScheduleConfig() {
  afh::ScheduleConfig config{};
  config.session_seed = static_cast<std::uint64_t>(PR1_AFH_SESSION_SEED);
  config.session_id = static_cast<std::uint16_t>(PR1_AFH_SESSION_ID);
  config.map_version = static_cast<std::uint16_t>(PR1_AFH_MAP_VERSION);
  config.map = afh::ChannelMap{};  // all 40 channels, no exclusion (Gate B)
  return config;
}

enum class HopEventKind : std::uint8_t {
  TxSent = 0,
  TxFailed = 1,
  RxOk = 2,
  RxCrc = 3,
  RxOther = 4,
  TimeoutAdvance = 5,
  ResyncEnter = 6,
  Lock = 7,
  RxJump = 8,  // good packet while locked, but later than the expected frame (seq = frames skipped)
  // Gate C1 timing diagnostics (seq = microseconds, saturated at 65535).
  TxLate = 9,    // TX started > PR1_DIAG_TX_LATE_US after its gap ended
  TxSlow = 10,   // blocking transmit took > PR1_DIAG_TX_SLOW_US longer than the run minimum
  RxLate = 11,   // RX-done arrived > PR1_DIAG_RX_LATE_US after the frame grid (checked in idle)
  RxSlowReady = 12,  // IRQ -> re-armed took > PR1_DIAG_RX_READY_US (checked in idle)
  // Breakdown of the same slow frame: t_us = IRQ->SPI start, logical = SPI read,
  // seq = SPI end->re-arm start (decode + follower), rssi = re-arm duration (all us).
  RxSlowBreakdown = 13,
  IdleOverlap = 14,  // an RX IRQ arrived during idle work: seq = work us, ch = work kind
  ColdWork = 15,     // diagnostic only: cold (rarely run) code executed in idle, seq = us
};

struct HopEvent {
  std::uint32_t t_us = 0;
  std::uint32_t logical_lo = 0;
  std::uint16_t raw_sequence = 0;
  std::uint8_t channel = 0;
  HopEventKind kind = HopEventKind::TxSent;
  std::int16_t rssi_dbm = 0;
};

struct HopTelemetry {
  static constexpr std::size_t kRingSize = 256;

  std::uint32_t retunes = 0;
  std::uint32_t retune_failures = 0;
  std::uint32_t timeout_advances = 0;
  std::uint32_t resync_entries = 0;
  std::uint32_t locks = 0;
  std::uint32_t schedule_agree = 0;
  std::uint32_t schedule_disagree = 0;
  std::uint32_t standby_failures = 0;
  std::uint8_t current_channel = 0;
  bool locked = false;
  std::uint64_t logical = 0;  // TX: next logical frame; RX: next expected logical frame
  std::uint32_t period_est_us = 0;
  std::uint32_t consecutive_timeouts = 0;
  std::uint32_t max_consecutive_timeouts = 0;
  // Initial acquisition (two packets on the rendezvous channel).
  std::uint32_t acq_anchors = 0;     // packets used as (re-)anchors before acquisition
  std::uint32_t acq_crc = 0;         // CRC-bad before acquisition (outside the span)
  std::uint16_t acq_frames = 0;      // frame spacing that produced the period
  std::uint32_t acq_done_us = 0;     // RX time acquisition completed
  std::array<std::uint32_t, afh::kChannelCount> channel_ok{};
  std::array<std::uint32_t, afh::kChannelCount> channel_crc{};
  instrumentation::DurationWindow<64> retune_us{};
  instrumentation::DurationWindow<64> hop_compute_us{};

  std::array<HopEvent, kRingSize> ring{};
  std::uint32_t ring_written = 0;

  // Anomalies only (timeouts, resync, lock, CRC, duplicates): small enough to keep a
  // whole short run, so early-run losses are not overwritten by later RxOk/TxSent.
  static constexpr std::size_t kAnomalySize = 160;
  std::array<HopEvent, kAnomalySize> anomalies{};
  std::uint32_t anomalies_written = 0;

  void record(const HopEvent& event) {
    ring[ring_written % kRingSize] = event;
    ++ring_written;
    if (event.kind != HopEventKind::TxSent && event.kind != HopEventKind::RxOk) {
      if (anomalies_written < kAnomalySize) anomalies[anomalies_written] = event;
      ++anomalies_written;
    }
  }
};

}  // namespace pr1::runtime::afhrt
