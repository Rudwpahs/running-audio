#pragma once

#include <array>
#include <atomic>
#include <cinttypes>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>

#include "../../common/pr1_packet.hpp"
#include "../../common/pr1_sequence.hpp"
#include "pr1_adaptive_map_runtime.hpp"
#include "pr1_afh_runtime.hpp"
#include "pr1_ctrl_plane.hpp"
#include "../../common/pr1_placement.hpp"
#include "pr1_live_metrics.hpp"
#include "pr1_live_profile.hpp"
#include "pr1_radio_port.hpp"

#ifndef PR1_DIAG_TX_LATE_US
#define PR1_DIAG_TX_LATE_US 30
#endif
#ifndef PR1_DIAG_TX_SLOW_US
#define PR1_DIAG_TX_SLOW_US 40
#endif
#ifndef PR1_DIAG_RX_LATE_US
#define PR1_DIAG_RX_LATE_US 60
#endif
// RX: the control core may touch USB only from RX re-armed until this long before the
// next expected RX-done (core-0 USB work measurably slows the radio core's post-read path).
#ifndef PR1_CTRL_WINDOW_GUARD_US
#define PR1_CTRL_WINDOW_GUARD_US 600
#endif

// Diagnostic only (C1 cache test): every N frames run rarely-used code (text formatting,
// no I/O, no state change) in the RX idle window, then see whether the next post-read
// path slows down. 0 = off.
#ifndef PR1_DIAG_COLD_WORK_EVERY
#define PR1_DIAG_COLD_WORK_EVERY 0
#endif

#ifndef PR1_DIAG_RX_READY_US
#define PR1_DIAG_RX_READY_US 1950
#endif

namespace pr1::runtime {

class FixedLinkRuntime {
 public:
  static constexpr bool kAfhEnabled = afh::kEnabledByDefault;
  static constexpr bool kAdaptive = kAfhEnabled && amap::kEnabled;
  // Gate C1: the control plane lives on the other core; the radio loop only
  // exchanges fixed-size records with it (no parsing, printf or Serial here).
  using CtrlSink = bool (*)(const ctrl::Out& out);
  using CtrlSource = bool (*)(ctrl::In* in);

  FixedLinkRuntime(RadioPort& radio,
                   RuntimeRole role,
                   FixedFlrcProfile profile = kFixedFlrcProfile,
                   std::uint16_t stream_id = 1U)
      : radio_(radio), role_(role), profile_(profile), stream_id_(stream_id) {}

  bool begin() {
    if (role_ == RuntimeRole::Safe) return false;
    if (!radio_.beginFixedFlrc(profile_)) return false;

    if (role_ == RuntimeRole::Rx) {
      radio_.setRxIrqHandler(&FixedLinkRuntime::rxIrqThunk, this);
      if constexpr (kAfhEnabled) {
        // Unlocked start: park on a rendezvous channel until any packet is heard.
        if (!retune(scheduler_.rendezvousChannel(resync_slot_))) return false;
      }
      if (!radio_.startReceive()) return false;
    } else {
      next_tx_allowed_us_ = radio_.nowMicros();
    }

    initialized_ = true;
    return true;
  }

  void tick(std::uint32_t now_us) {
    if (!initialized_) return;
    if (role_ == RuntimeRole::Tx) {
      if constexpr (kAdaptive) {
        // Control records are applied only while no transmission is due.
        if (!deadlineReached(now_us, next_tx_allowed_us_)) {
          serviceTxControl();
          return;
        }
      }
      serviceTx(now_us);
    } else if (role_ == RuntimeRole::Rx) {
      if (rx_pending_) {
        serviceRx();
      } else if constexpr (kAfhEnabled) {
        serviceHopTimeout(now_us);
      } else {
        // Fixed-channel RX has no hop deadline to publish its idle window.
        ctrl_window_open_.store(true, std::memory_order_release);
        // An RX IRQ between the rx_pending_ check and the store must not leave the
        // window open through the post-read path.
        if (rx_pending_) ctrl_window_open_.store(false, std::memory_order_release);
      }
    }
  }

  const LiveMetrics& metrics() const { return metrics_; }
  const afhrt::HopTelemetry& hopTelemetry() const { return hop_; }
  const afh::Scheduler& hopScheduler() const { return scheduler_; }
  const amap::Telemetry& mapTelemetry() const { return amap_; }
  const quality::Estimator& estimator() const { return estimator_; }
  const amap::ProbeSlot& probeSlot() const { return probe_; }

  void setControlPlane(CtrlSink sink, CtrlSource source) {
    sink_ = sink;
    source_ = source;
  }
  using PayloadSource = bool (*)(std::uint32_t, std::uint8_t*, std::size_t);
  using PayloadSink = void (*)(const std::uint8_t*, std::size_t);
  void setPayloadHooks(PayloadSource source, PayloadSink sink) {
    payload_source_ = source; payload_sink_ = sink;
  }
  bool initialized() const { return initialized_; }

  // Called from the control core (a plain internal-RAM flag read: no timer or
  // peripheral access while waiting). TX: always open (a late TX start only shifts the
  // whole timeline). RX: the radio core's idle loop sets/clears it around each frame.
  PR1_IRAM bool controlWindowOpen() const {
    if (role_ != RuntimeRole::Rx) return true;
    return ctrl_window_open_.load(std::memory_order_acquire);
  }

 private:
  static void rxIrqThunk(void* context, std::uint32_t timestamp_us) {
    if (context == nullptr) return;
    auto* self = static_cast<FixedLinkRuntime*>(context);
    // Single-producer ISR contract: RX is not re-armed until the pending event
    // is serviced in tick(), so at most one receive-complete event is pending.
    self->rx_irq_timestamp_us_ = timestamp_us;
    self->rx_pending_ = true;
    self->ctrl_window_open_.store(false, std::memory_order_release);  // post-read path starts
  }

  static bool deadlineReached(std::uint32_t now_us, std::uint32_t due_us) {
    return static_cast<std::int32_t>(now_us - due_us) >= 0;
  }

  void serviceTx(std::uint32_t now_us) {
    if (!deadlineReached(now_us, next_tx_allowed_us_)) return;

    const std::uint32_t lateness_us = now_us - next_tx_allowed_us_;
    if (profile_.tx_gap_us > 0U && lateness_us >= profile_.tx_gap_us) {
      metrics_.onSchedulerMiss(radio_.nowMicros(), tx_sequence_);
    }

    std::array<std::uint8_t, pr1::kDartTargetOpusPayloadBytes> payload{};
    for (std::size_t i = 0; i < payload.size(); ++i) {
      payload[i] = static_cast<std::uint8_t>((tx_sequence_ + i) & 0xFFU);
    }

    if (payload_source_ && !payload_source_(now_us, payload.data(), payload.size())) {
      next_tx_allowed_us_ = radio_.nowMicros() + profile_.tx_gap_us;  // no hot retry loop
      return;
    }

    pr1::Header header{};
    header.stream_id = stream_id_;
    header.sequence = tx_sequence_;
    header.sample_rate = pr1::kDartSampleRateHz;
    header.capture_ms = now_us / 1000U;

    const std::size_t packet_len =
        pr1::encode_packet(header, payload.data(), payload.size(), tx_buffer_.data(),
                           tx_buffer_.size());
    if (packet_len != pr1::kDartPacketBytes) {
      initialized_ = false;
      return;
    }

    std::uint8_t tx_channel = 0U;
    if constexpr (kAfhEnabled) {
      // The radio is in standby here (begin / previous blocking transmit).
      const std::uint32_t compute_start_us = radio_.nowMicros();
      tx_channel = channelFor(hop_.logical);
      hop_.hop_compute_us.observe(radio_.nowMicros() - compute_start_us);
      if (!retune(tx_channel)) {
        next_tx_allowed_us_ = radio_.nowMicros() + profile_.tx_gap_us;
        return;
      }
      if constexpr (PR1_AFH_TX_SETTLE_US > 0) {
        // Diagnostic: idle after the frequency write before the blocking transmit.
        const std::uint32_t settle_start_us = radio_.nowMicros();
        while (radio_.nowMicros() - settle_start_us < static_cast<std::uint32_t>(PR1_AFH_TX_SETTLE_US)) {
        }
      }
    }

    metrics_.onTxQueued(radio_.nowMicros(), tx_sequence_);
    const std::uint32_t tx_start_us = radio_.nowMicros();
    metrics_.onTxStart(tx_start_us, tx_sequence_);
    const bool sent = radio_.transmit(tx_buffer_.data(), packet_len);
    const std::uint32_t tx_attempt_done_us = radio_.nowMicros();
    if constexpr (kAfhEnabled) {
      // Timing diagnostics (C1): late start (CPU) and slow blocking transmit.
      const std::uint32_t tx_us = tx_attempt_done_us - tx_start_us;
      if (sent && tx_us < tx_min_us_) tx_min_us_ = tx_us;
      if (lateness_us > static_cast<std::uint32_t>(PR1_DIAG_TX_LATE_US)) {
        hop_.record({now_us, static_cast<std::uint32_t>(hop_.logical), sat16(lateness_us), tx_channel,
                     afhrt::HopEventKind::TxLate, 0});
      }
      if (sent && tx_us > tx_min_us_ + static_cast<std::uint32_t>(PR1_DIAG_TX_SLOW_US)) {
        hop_.record({tx_attempt_done_us, static_cast<std::uint32_t>(hop_.logical), sat16(tx_us - tx_min_us_),
                     tx_channel, afhrt::HopEventKind::TxSlow, 0});
      }
      hop_.record({tx_attempt_done_us, static_cast<std::uint32_t>(hop_.logical), tx_sequence_,
                   tx_channel,
                   sent ? afhrt::HopEventKind::TxSent : afhrt::HopEventKind::TxFailed, 0});
      if (sent) ++hop_.channel_ok[tx_channel];
    }
    if (sent) {
      metrics_.onTxDone(tx_attempt_done_us, tx_sequence_);
      tx_sequence_ = static_cast<std::uint16_t>(tx_sequence_ + 1U);
      if constexpr (kAfhEnabled) ++hop_.logical;
    }

    // Historical V4 semantics: TX_GAP_US is idle time after the blocking TX
    // call completes. Do not turn it into a start-to-start packet period.
    // A failed attempt also observes the gap to avoid a hot retry loop.
    next_tx_allowed_us_ = tx_attempt_done_us + profile_.tx_gap_us;
  }

  void serviceRx() {
    const std::uint32_t irq_timestamp_us = rx_irq_timestamp_us_;
    rx_pending_ = false;

    const std::uint32_t label_sequence =
        sequence_unwrapper_.initialized()
            ? static_cast<std::uint32_t>(sequence_unwrapper_.rawReference())
            : 0U;
    metrics_.onRxIrq(irq_timestamp_us, label_sequence);
    metrics_.onQueueDepth(1U, radio_.nowMicros(), label_sequence);

    std::size_t packet_len = 0;
    const std::uint32_t spi_start_us = radio_.nowMicros();
    metrics_.onSpiStart(spi_start_us, label_sequence);
    const RadioReadResult read_result =
        radio_.readPacket(rx_buffer_.data(), rx_buffer_.size(), &packet_len);
    const std::uint32_t spi_end_us = radio_.nowMicros();
    metrics_.onSpiEnd(spi_end_us, label_sequence);

    const std::uint8_t* audio_payload = nullptr;
    bool hop_decoded = false;
    std::uint16_t hop_raw = 0U;
    std::int16_t hop_rssi = 0;
    if (read_result == RadioReadResult::CrcError) {
      if (!kAfhEnabled || hop_acquired_) {
        metrics_.onRxCrcFail(spi_end_us, label_sequence);
      } else {
        ++hop_.acq_crc;  // before acquisition: not part of the measured span
      }
    } else if (read_result == RadioReadResult::Ok) {
      pr1::DecodedPacket decoded{};
      const bool canonical_length = packet_len == pr1::kDartPacketBytes;
      const bool decoded_ok =
          canonical_length && pr1::decode_packet(rx_buffer_.data(), packet_len, &decoded);
      if (decoded_ok && decoded.header.stream_id == stream_id_ &&
          decoded.header.sample_rate == pr1::kDartSampleRateHz &&
          decoded.header.payload_len == pr1::kDartTargetOpusPayloadBytes) {
        audio_payload = decoded.payload;
        const std::uint32_t packet_done_us = radio_.nowMicros();
        const std::int16_t rssi = radio_.rssiDbm();
        // AFH: the measured span starts once acquisition has a period (see acquire()).
        if (!kAfhEnabled || acquire(decoded.header.sequence, irq_timestamp_us)) {
          metrics_.onRxPacket(packet_done_us, decoded.header.sequence, rssi);
          observeSequence(decoded.header.sequence);
        }
        if constexpr (kAfhEnabled) {
          hop_decoded = true;
          hop_raw = decoded.header.sequence;
          hop_rssi = rssi;
        }
      }
    }

    if constexpr (kAfhEnabled) {
      followAfterRx(read_result, hop_decoded, hop_raw, hop_rssi, irq_timestamp_us);
    }

    const std::uint32_t rearm_start_us = radio_.nowMicros();
    metrics_.onRxRearmStart(rearm_start_us, label_sequence);
    const bool rearmed = radio_.startReceive();
    const std::uint32_t rearm_done_us = radio_.nowMicros();
    metrics_.onRxRearmDone(rearm_done_us, label_sequence);
    // C1 diagnostic: two stores only; the threshold check runs in the idle loop.
    diag_ready_us_ = rearm_done_us - irq_timestamp_us;
    diag_ready_logical_ = hop_.logical;
    diag_irq_to_spi_ = spi_start_us - irq_timestamp_us;
    diag_spi_ = spi_end_us - spi_start_us;
    diag_proc_ = rearm_start_us - spi_end_us;
    diag_rearm_ = rearm_done_us - rearm_start_us;
    metrics_.onQueueDepth(0U, rearm_done_us, label_sequence);
    if (!rearmed) initialized_ = false;
    // Never decode or write I2S on this path. The sink copies to internal RAM only,
    // AFTER re-arm; rx_buffer_ stays valid until the next tick.
    if (rearmed && audio_payload && payload_sink_)
      payload_sink_(audio_payload, pr1::kDartTargetOpusPayloadBytes);
  }

  void observeSequence(std::uint16_t raw_sequence) {
    if (!sequence_unwrapper_.initialized()) {
      sequence_unwrapper_.reset(raw_sequence, 0U);
      metrics_.onMissing(0U);
      return;
    }

    const auto preview = sequence_unwrapper_.preview(raw_sequence);
    if (preview.status != pr1::sequence::UnwrapStatus::Ok || !preview.forward) return;

    const auto latest = sequence_unwrapper_.latest();
    const auto gap = preview.index > latest ? preview.index - latest - 1U : 0U;
    metrics_.onMissing(static_cast<std::uint32_t>(gap));
    sequence_unwrapper_.acceptForward(preview);
  }

  // --- AFH (Gate B) -------------------------------------------------------

  // Channel for a logical frame on the live timeline (monotonic callers only:
  // TX next frame, RX next expected frame). Applies a staged map at its activation
  // frame (Gate C) and the probe override for the single reserved probe frame.
  PR1_IRAM std::uint8_t channelFor(std::uint64_t logical) {
    if constexpr (kAdaptive) {
      if (scheduler_.pending().valid && logical >= scheduler_.pending().activation_sequence) {
        prev_config_ = scheduler_.current();
        prev_activation_ = scheduler_.pending().activation_sequence;
        prev_valid_ = true;
        scheduler_.applyPendingIfDue(logical);
        ++amap_.activations;
        amap_.logActivation(scheduler_.current().map_version, prev_activation_, logical);
        const std::uint8_t active = scheduler_.current().map.activeCount();
        if (active < amap_.min_active_seen) amap_.min_active_seen = active;
        amap_.record({nowMs(), static_cast<std::uint32_t>(logical),
                      static_cast<std::uint32_t>(prev_activation_), scheduler_.current().map.bits, 0U,
                      scheduler_.current().map_version, 0U, active, amap::EventKind::Activated});
      }
      if (probe_.valid) {
        if (logical == probe_.at) return probe_.channel;
        if (logical > probe_.at && role_ == RuntimeRole::Tx) probe_.valid = false;
      }
    }
    return scheduler_.channelForSequence(logical);
  }

  // Channel a past frame used (idle-time attribution only; never on the hot path).
  PR1_IRAM std::uint8_t channelForPast(std::uint64_t logical) const {
    if (prev_valid_ && logical < prev_activation_) {
      const afh::Scheduler previous(prev_config_);
      return previous.channelForSequence(logical);
    }
    return scheduler_.channelForSequence(logical);
  }

  PR1_IRAM std::uint32_t nowMs() const { return radio_.nowMicros() / 1000U; }

  bool retune(std::uint8_t channel) {
    const std::uint32_t start_us = radio_.nowMicros();
#if PR1_AFH_DIAG_SAME_FREQ || (PR1_AFH_DIAG_FIXED_CHANNEL >= 0)
    // Diagnostic only: run the full hop path (schedule + SetRfFrequency) but always
    // program one fixed channel, separating "retune action" from "which frequency".
    constexpr std::uint8_t kDiagChannel =
        PR1_AFH_DIAG_FIXED_CHANNEL >= 0 ? static_cast<std::uint8_t>(PR1_AFH_DIAG_FIXED_CHANNEL) : 0U;
    const bool ok = radio_.setFrequencyHz(afh::frequencyHz(kDiagChannel));
#else
    const bool ok = radio_.setFrequencyHz(afh::frequencyHz(channel));
#endif
    hop_.retune_us.observe(radio_.nowMicros() - start_us);
    ++hop_.retunes;
    if (!ok) {
      ++hop_.retune_failures;
      return false;
    }
    hop_.current_channel = channel;
    return true;
  }

  // Hop logical frame for a heard wire sequence: the value congruent to `raw`
  // (mod 2^16) nearest to the expected frame, so 16-bit wraps and outages up to
  // +/-32767 frames keep TX and RX on the same timeline. Before anything was
  // heard the TX timeline is assumed to start at 0 (RX reset after TX boot).
  std::uint64_t hopLogicalFor(std::uint16_t raw) const {
    if (!hop_ever_heard_) return raw;
    const std::uint64_t ref = hop_.logical;
    const auto diff = static_cast<std::int16_t>(raw - static_cast<std::uint16_t>(ref));
    if (diff < 0 && static_cast<std::uint64_t>(-static_cast<std::int32_t>(diff)) > ref) return raw;
    return ref + static_cast<std::int64_t>(diff);
  }

  // Initial acquisition (Gate B pre-C fix): stay parked on the rendezvous channel
  // until two packets are heard there; their spacing gives the frame period, so
  // the follower never starts without a loss deadline (the 11-frame start-up skip
  // came from losing the frame after a period-less lock). Returns true when this
  // packet belongs to the measured span.
  bool acquire(std::uint16_t raw, std::uint32_t irq_us) {
    if (hop_acquired_) return true;
    const auto frames = static_cast<std::uint16_t>(raw - acq_raw_);
    if (acq_anchor_valid_ && frames >= 1U && frames <= 120U) {
      hop_.period_est_us = (irq_us - acq_irq_us_) / frames;
      hop_.acq_frames = frames;
      hop_.acq_done_us = irq_us;
      hop_acquired_ = true;
      return true;
    }
    acq_anchor_valid_ = true;  // first packet, or spacing out of range: re-anchor
    acq_raw_ = raw;
    acq_irq_us_ = irq_us;
    ++hop_.acq_anchors;
    return false;
  }

  // Margin after the predicted RX-done time before declaring the frame lost.
  // Must stay well below the on-air idle time so the retune lands before the
  // next preamble (P - airtime ~1.9 ms at 150 us gap).
  static std::uint32_t lossMargin(std::uint32_t period) {
    constexpr std::uint32_t kMin = PR1_AFH_LOSS_MARGIN_MIN_US;
    constexpr std::uint32_t kMax = PR1_AFH_LOSS_MARGIN_MAX_US;
    const std::uint32_t m = period / 8U;
    return m < kMin ? kMin : (m > kMax ? kMax : m);
  }

  // Deadline for the expected frame hop_.logical, on the grid anchored at the last
  // good RX-done. A CRC-bad (possibly truncated) packet never moves the anchor.
  void armGridDeadline() {
    const std::uint32_t period = hop_.period_est_us;
    if (period == 0U || !last_rx_valid_ || hop_.logical <= last_rx_logical_) {
      hop_deadline_valid_ = false;  // no cadence yet: wait on this channel instead
      return;
    }
    const auto frames = static_cast<std::uint32_t>(hop_.logical - last_rx_logical_);
    hop_deadline_us_ = last_rx_irq_us_ + frames * period + lossMargin(period);
    hop_deadline_valid_ = true;
  }

  std::uint8_t timedChannelFor(std::uint64_t logical) {
    const std::uint32_t start_us = radio_.nowMicros();
    const std::uint8_t channel = channelFor(logical);
    hop_.hop_compute_us.observe(radio_.nowMicros() - start_us);
    return channel;
  }

  // Called after every receive-complete, with the radio in standby (readData).
  void followAfterRx(RadioReadResult result, bool decoded, std::uint16_t raw, std::int16_t rssi,
                     std::uint32_t irq_us) {
    const std::uint8_t heard_channel = hop_.current_channel;
    if (decoded && !hop_acquired_) {
      // Acquisition anchor only: stay parked on this channel (re-armed by serviceRx).
      hop_.record({irq_us, raw, raw, heard_channel, afhrt::HopEventKind::RxOther, rssi});
      return;
    }
    if (decoded) {
      const std::uint64_t logical = hopLogicalFor(raw);
      if (hop_.locked && logical < hop_.logical) {
        // Duplicate / older frame: keep channel and deadline unchanged.
        hop_.record({irq_us, static_cast<std::uint32_t>(logical), raw, heard_channel,
                     afhrt::HopEventKind::RxOther, rssi});
        return;
      }
      // Tuned to the expected frame's channel by construction when locked and on time.
      // Any other case is checked against the schedule later, from the idle loop: a
      // schedule computation here (~45 us) costs the next preamble at a 150 us gap.
      const bool on_expected = hop_.locked && logical == hop_.logical;
      if (on_expected) {
        ++hop_.schedule_agree;
      } else {
        deferred_check_valid_ = true;
        deferred_check_logical_ = logical;
        deferred_check_channel_ = heard_channel;
      }
      ++hop_.channel_ok[heard_channel];
      if (hop_.locked && last_rx_valid_ && logical > last_rx_logical_ &&
          logical - last_rx_logical_ <= 4U) {
        const std::uint32_t sample =
            (irq_us - last_rx_irq_us_) / static_cast<std::uint32_t>(logical - last_rx_logical_);
        hop_.period_est_us = hop_.period_est_us == 0U
                                 ? sample
                                 : hop_.period_est_us - hop_.period_est_us / 8U + sample / 8U;
      }
      if (hop_.locked && logical > hop_.logical) {
        // Frames between the expected one and this packet were lost without a
        // timeout or CRC event; log them so per-run loss can be fully attributed.
        // Channel of the skipped frames is reconstructed offline from the schedule
        // (255 = not computed on the hot path).
        hop_.record({irq_us, static_cast<std::uint32_t>(hop_.logical),
                     static_cast<std::uint16_t>(logical - hop_.logical), 255U,
                     afhrt::HopEventKind::RxJump, rssi});
      }
      if (!hop_.locked) {
        ++hop_.locks;
        hop_.record({irq_us, static_cast<std::uint32_t>(logical), raw, heard_channel,
                     afhrt::HopEventKind::Lock, rssi});
      }
      hop_.record({irq_us, static_cast<std::uint32_t>(logical), raw, heard_channel,
                   afhrt::HopEventKind::RxOk, rssi});
      if constexpr (kAdaptive) {
        if (hop_.locked && logical > hop_.logical) {
          for (std::uint64_t f = hop_.logical; f < logical && f < hop_.logical + 4U; ++f) {
            queueOutcome(f, 255U, amap::OutcomeKind::Timeout, irq_us);
          }
        }
        queueOutcome(logical, heard_channel, amap::OutcomeKind::Ok, irq_us);
      }
      hop_.locked = true;
      hop_ever_heard_ = true;
      hop_.consecutive_timeouts = 0U;
      last_rx_logical_ = logical;
      last_rx_irq_us_ = irq_us;
      last_rx_valid_ = true;
      hop_.logical = logical + 1U;
    } else {
      if (result == RadioReadResult::CrcError) ++hop_.channel_crc[heard_channel];
      hop_.record({irq_us, static_cast<std::uint32_t>(hop_.logical), 0U, heard_channel,
                   result == RadioReadResult::CrcError ? afhrt::HopEventKind::RxCrc
                                                       : afhrt::HopEventKind::RxOther,
                   0});
      // Parked, or a non-CRC read error: re-arm on the same channel.
      if (!hop_.locked || result != RadioReadResult::CrcError) return;
      // Place the CRC-bad packet on the frame grid by time (it may be a truncated or
      // early-ending reception) and continue after that slot; never re-anchor on it.
      const std::uint32_t period = hop_.period_est_us;
      if (period > 0U && last_rx_valid_) {
        const std::uint32_t since = irq_us - last_rx_irq_us_;
        const std::uint64_t slot = last_rx_logical_ + (since + period / 2U) / period;
        if constexpr (kAdaptive) queueOutcome(slot, heard_channel, amap::OutcomeKind::Crc, irq_us);
        if (slot + 1U > hop_.logical) hop_.logical = slot + 1U;
      } else {
        ++hop_.logical;
      }
    }
    armGridDeadline();
    retune(timedChannelFor(hop_.logical));
  }

  // RX idle only (radio re-armed): window for the control core's USB work.
  void publishControlWindow(std::uint32_t now_us) {
    bool open = true;  // no frame timing (unlocked / acquiring): free
    if (hop_.locked && hop_deadline_valid_) {
      const std::uint32_t end =
          hop_deadline_us_ - lossMargin(hop_.period_est_us) - static_cast<std::uint32_t>(PR1_CTRL_WINDOW_GUARD_US);
      open = static_cast<std::int32_t>(end - now_us) > 0;
    }
    ctrl_window_open_.store(open, std::memory_order_release);
  }

  static std::uint16_t sat16(std::uint32_t v) { return v > 65535U ? 65535U : static_cast<std::uint16_t>(v); }

  // Idle-loop timing diagnostic (C1): flag a good packet whose RX-done is late on the
  // grid of the previous good packet (TX started late, or the IRQ was serviced late).
  void checkRxLateness() {
    if (diag_ready_us_ != 0U) {
      if (diag_ready_us_ > static_cast<std::uint32_t>(PR1_DIAG_RX_READY_US)) {
        hop_.record({radio_.nowMicros(), static_cast<std::uint32_t>(diag_ready_logical_), sat16(diag_ready_us_),
                     hop_.current_channel, afhrt::HopEventKind::RxSlowReady, 0});
        hop_.record({diag_irq_to_spi_, diag_spi_, sat16(diag_proc_), 0U, afhrt::HopEventKind::RxSlowBreakdown,
                     static_cast<std::int16_t>(diag_rearm_ > 32767U ? 32767U : diag_rearm_)});
      }
      diag_ready_us_ = 0U;
    }
    if (!last_rx_valid_ || last_rx_logical_ == diag_rx_logical_) return;
    if (diag_rx_valid_ && hop_.period_est_us > 0U && last_rx_logical_ > diag_rx_logical_ &&
        last_rx_logical_ - diag_rx_logical_ <= 4U) {
      const auto frames = static_cast<std::uint32_t>(last_rx_logical_ - diag_rx_logical_);
      const auto late = static_cast<std::int32_t>((last_rx_irq_us_ - diag_rx_irq_us_) - frames * hop_.period_est_us);
      if (late > PR1_DIAG_RX_LATE_US) {
        hop_.record({last_rx_irq_us_, static_cast<std::uint32_t>(last_rx_logical_),
                     sat16(static_cast<std::uint32_t>(late)), hop_.current_channel, afhrt::HopEventKind::RxLate, 0});
      }
    }
    diag_rx_valid_ = true;
    diag_rx_logical_ = last_rx_logical_;
    diag_rx_irq_us_ = last_rx_irq_us_;
  }

  // RX idle: if the predicted frame did not arrive, advance on the TX cadence.
  // Microseconds until the next expected RX-done (only meaningful when locked with a deadline).
  std::int32_t untilExpectedDone(std::uint32_t now_us) const {
    return static_cast<std::int32_t>(hop_deadline_us_ - lossMargin(hop_.period_est_us) - now_us);
  }

  void serviceHopTimeout(std::uint32_t now_us) {
    checkRxLateness();
    publishControlWindow(now_us);
    if constexpr (PR1_DIAG_COLD_WORK_EVERY > 0) {
      if (hop_.locked && hop_deadline_valid_ &&
          untilExpectedDone(now_us) >= static_cast<std::int32_t>(PR1_CTRL_WINDOW_GUARD_US) && hop_.logical % (PR1_DIAG_COLD_WORK_EVERY > 0 ? PR1_DIAG_COLD_WORK_EVERY : 1) == 0U &&
          hop_.logical != diag_cold_logical_) {
        diag_cold_logical_ = hop_.logical;
        const std::uint32_t t0 = radio_.nowMicros();
        char buf[224];
        std::uint32_t sink = 0U;
        for (std::uint8_t t = 0; t <= static_cast<std::uint8_t>(ctrl::OutType::Aborted); ++t) {
          ctrl::Out o{};
          o.type = static_cast<ctrl::OutType>(t);
          o.id = t; o.bits = hop_.logical * 0x9E37ULL; o.logical = hop_.logical; o.reason = "diag";
          sink += static_cast<std::uint32_t>(ctrl::format(o, buf, sizeof(buf)));
        }
        sink += static_cast<std::uint32_t>(std::snprintf(buf, sizeof(buf), "%.3f %e %llx",
            static_cast<double>(hop_.period_est_us) / 7.0, static_cast<double>(sink),
            static_cast<unsigned long long>(hop_.logical)));
        hop_.record({t0, static_cast<std::uint32_t>(hop_.logical), sat16(radio_.nowMicros() - t0),
                     static_cast<std::uint8_t>(sink & 0xFFU), afhrt::HopEventKind::ColdWork, 0});
        return;
      }
    }
    if constexpr (kAdaptive) {
      if (!deadlineNear(now_us)) {
        const std::uint32_t work_start_us = radio_.nowMicros();
        const std::uint8_t kind = serviceQuality();
        if (kind != 0U) {
          if (rx_pending_) {
            // Idle work overlapped a reception: the post-read path started late.
            hop_.record({work_start_us, static_cast<std::uint32_t>(hop_.logical),
                         sat16(radio_.nowMicros() - work_start_us), kind, afhrt::HopEventKind::IdleOverlap, 0});
          }
          return;
        }
      }
    }
    if (deferred_check_valid_) {
      // Idle loop, RX already armed: verify an off-expected packet against the schedule.
      deferred_check_valid_ = false;
      if (channelForPast(deferred_check_logical_) == deferred_check_channel_ ||
          (kAdaptive && ((deferred_check_logical_ == last_probe_at_ &&
                          deferred_check_channel_ == last_probe_channel_) ||
                         (probe_.valid && deferred_check_logical_ == probe_.at &&
                          deferred_check_channel_ == probe_.channel)))) {
        ++hop_.schedule_agree;
      } else {
        ++hop_.schedule_disagree;
      }
      return;
    }
    if (!hop_.locked || !hop_deadline_valid_ || !deadlineReached(now_us, hop_deadline_us_)) return;
    if (!radio_.standby()) {
      ++hop_.standby_failures;
      initialized_ = false;
      return;
    }
    if (rx_pending_) return;  // a packet completed meanwhile: serviceRx reads it from standby
    const std::uint32_t period = hop_.period_est_us;
    // Every frame whose predicted RX-done (+margin) has passed is lost; jump past all of
    // them at once so a loop stall cannot leave the follower behind the TX.
    const std::uint32_t frames = 1U + (now_us - hop_deadline_us_) / period;
    hop_.logical += frames;
    armGridDeadline();
    hop_.timeout_advances += frames;
    hop_.consecutive_timeouts += frames;
    if (hop_.consecutive_timeouts > hop_.max_consecutive_timeouts) {
      hop_.max_consecutive_timeouts = hop_.consecutive_timeouts;
    }
    if (hop_.consecutive_timeouts > static_cast<std::uint32_t>(PR1_AFH_RESYNC_AFTER_MISSES)) {
      hop_.locked = false;
      hop_deadline_valid_ = false;
      ++hop_.resync_entries;
      ++resync_slot_;
      const std::uint8_t park = scheduler_.rendezvousChannel(resync_slot_);
      hop_.record({now_us, static_cast<std::uint32_t>(hop_.logical), 0U, park,
                   afhrt::HopEventKind::ResyncEnter, 0});
      retune(park);
    } else {
      // Event channel = the first frame that was missed (not the next channel), so
      // per-channel loss can be binned directly from the log.
      const std::uint64_t first_missed = hop_.logical - frames;
      if constexpr (kAdaptive) {
        for (std::uint64_t f = first_missed; f < hop_.logical && f < first_missed + 4U; ++f) {
          queueOutcome(f, 255U, amap::OutcomeKind::Timeout, now_us);
        }
      }
      hop_.record({now_us, static_cast<std::uint32_t>(first_missed),
                   static_cast<std::uint16_t>(frames), channelForPast(first_missed),
                   afhrt::HopEventKind::TimeoutAdvance, 0});
      retune(timedChannelFor(hop_.logical));
    }
    if (!radio_.startReceive()) initialized_ = false;
  }

  // --- Gate C: adaptive map (RX decides, host relays, both apply at frame N) -----

  PR1_IRAM void queueOutcome(std::uint64_t logical, std::uint8_t channel, amap::OutcomeKind kind,
                    std::uint32_t t_us) {
    amap::Outcome o{};
    o.logical_lo = static_cast<std::uint32_t>(logical);
    o.t_ms = t_us / 1000U;
    o.kind = kind;
    o.probe = probe_.valid && logical == probe_.at;
    o.channel = o.probe ? probe_.channel : channel;
    if (!outcomes_.push(o)) ++amap_.outcomes_dropped;
  }

  // True unless the next expected RX-done is at least 400 us away. Idle work
  // (estimator, proposals, control lines, prints) must never overlap the next
  // reception: ~90 us of extra latency there caused a self-sustaining loss mode.
  bool deadlineNear(std::uint32_t now_us) const {
    if (!hop_deadline_valid_) return false;
    const std::uint32_t expected_done = hop_deadline_us_ - lossMargin(hop_.period_est_us);
    return static_cast<std::int32_t>(expected_done - now_us) < 400;
  }

  PR1_IRAM std::uint64_t logicalFromLo(std::uint32_t lo) const {
    // Outcomes are at most a few frames old: rebuild the 64-bit frame near hop_.logical.
    const std::uint64_t ref = hop_.logical;
    const auto diff = static_cast<std::int32_t>(lo - static_cast<std::uint32_t>(ref));
    return ref + static_cast<std::int64_t>(diff);
  }

  PR1_IRAM void serviceTxControl() {
    ctrl::In m{};
    if (source_ == nullptr || !source_(&m)) return;
    const std::uint32_t start_us = radio_.nowMicros();
    txControl(m);
    amap_.ctrl_us.observe(radio_.nowMicros() - start_us);
  }

  // Returns the kind of work done (0 none, 1 control, 2 outcome, 3 probe outcome, 4 proposal/probe upkeep).
  PR1_IRAM std::uint8_t serviceQuality() {
    ctrl::In m{};
    if (source_ != nullptr && source_(&m)) {
      if (m.type == ctrl::InType::Ack) {
        const std::uint32_t start_us = radio_.nowMicros();
        rxControl(m);
        amap_.ctrl_us.observe(radio_.nowMicros() - start_us);
      }
      return 1U;
    }
    amap::Outcome o{};
    if (outcomes_.pop(&o)) {
      const std::uint64_t logical = logicalFromLo(o.logical_lo);
      const std::uint8_t ch = o.channel == 255U ? channelForPast(logical) : o.channel;
      if (ch >= afh::kChannelCount) return 2U;
      const bool ok = o.kind == amap::OutcomeKind::Ok;
      auto& c = amap_.channels[ch];
      if (o.kind == amap::OutcomeKind::Ok) ++c.ok;
      if (o.kind == amap::OutcomeKind::Crc) ++c.crc;
      if (o.kind == amap::OutcomeKind::Timeout) ++c.timeout;
      if (o.probe) {
        ok ? ++c.probe_ok : ++c.probe_fail;
        if (estimator_.observeProbe(ch, ok, o.t_ms)) {
          const bool back = estimator_.channel(ch).state == quality::ChannelState::Active;
          amap_.record({o.t_ms, o.logical_lo, o.logical_lo, 0U, 0U,
                        scheduler_.current().map_version, ch, static_cast<std::uint8_t>(ok ? 1U : 0U),
                        amap::EventKind::ProbeResult});
          if (back) {
            ++c.reinclusions;
            amap_.record({o.t_ms, o.logical_lo, 0U, 0U, 0U, scheduler_.current().map_version, ch,
                          estimator_.activeCount(), amap::EventKind::Reincluded});
          }
        }
        if (logical >= probe_.at) {
          last_probe_at_ = probe_.at;
          last_probe_channel_ = probe_.channel;
          probe_.valid = false;
        }
        return 3U;
      }
      const auto before = estimator_.channel(ch).state;
      estimator_.observeData(ch, ok, o.t_ms);
      const auto after = estimator_.channel(ch).state;
      if (before != after) {
        amap::EventKind kind = amap::EventKind::Suspect;
        std::uint8_t value = 0U;
        if (after == quality::ChannelState::Excluded) {
          kind = amap::EventKind::Excluded;
          value = estimator_.channel(ch).consecutive_losses >= estimator_.config().exclude_losses
                      ? 1U : 2U;  // 1 losses, 2 pdr
          ++c.exclusions;
          c.last_excluded_ms = o.t_ms;
        } else if (after == quality::ChannelState::Active) {
          kind = amap::EventKind::Recovered;
        } else {
          ++c.suspects;
        }
        amap_.record({o.t_ms, o.logical_lo, 0U, 0U, 0U, scheduler_.current().map_version, ch, value, kind});
      }
      return 2U;
    }
    return maybePropose() ? 4U : 0U;
  }

  PR1_IRAM void emit(const ctrl::Out& out) {
    if (sink_ == nullptr || !sink_(out)) ++amap_.emit_dropped;
  }

  PR1_IRAM void abandonProposal(const char* reason) {
    ctrl::Out out{};
    out.type = ctrl::OutType::CommitAbandon;
    out.id = proposal_.id;
    out.reason = reason;
    out.logical = hop_.logical;
    proposal_.type = amap::ProposalType::None;
    next_proposal_ms_ = nowMs() + 1000U;  // back off after any failed proposal
    emit(out);
  }

  PR1_IRAM bool maybePropose() {
    if (probe_.valid && hop_.logical > probe_.at + 16U) {
      // The probe frame's outcome never reached the queue (e.g. a long stall):
      // count it as a failed probe so the estimator backs off and probing continues.
      if (estimator_.observeProbe(probe_.channel, false, nowMs())) {
        ++amap_.channels[probe_.channel].probe_fail;
        amap_.record({nowMs(), static_cast<std::uint32_t>(probe_.at), static_cast<std::uint32_t>(probe_.at),
                      0U, probe_.id, scheduler_.current().map_version, probe_.channel, 0U,
                      amap::EventKind::ProbeResult});
      }
      last_probe_at_ = probe_.at;
      last_probe_channel_ = probe_.channel;
      probe_.valid = false;
      return true;
    }
    if (proposal_.type != amap::ProposalType::None) {
      // Never commit after the guard: an expired proposal is dropped on the RX side.
      if (proposal_.activation <= hop_.logical + PR1_MAP_GUARD_FRAMES) {
        ++amap_.expired;
        amap_.record({nowMs(), static_cast<std::uint32_t>(hop_.logical),
                      static_cast<std::uint32_t>(proposal_.activation), proposal_.bits, proposal_.id,
                      proposal_.version, proposal_.channel, 0U, amap::EventKind::Expired});
        abandonProposal("expired");
        return true;
      }
      return false;
    }
    if (!hop_.locked || scheduler_.pending().valid) return false;
    const std::uint32_t now_ms = nowMs();
    if (static_cast<std::int32_t>(now_ms - next_proposal_ms_) < 0) return false;
    const afh::ChannelMap desired = estimator_.activeMap();
    const afh::ScheduleConfig& cur = scheduler_.current();
    ctrl::Out line{};
    if (desired.bits != cur.map.bits && desired.isValid()) {
      proposal_ = {amap::ProposalType::Map, ++next_id_, static_cast<std::uint16_t>(cur.map_version + 1U),
                   desired.bits, hop_.logical + PR1_MAP_LEAD_FRAMES, 0U};
      line.type = ctrl::OutType::PropMap;
      line.id = proposal_.id;
      line.version = proposal_.version;
      line.old_version = cur.map_version;
      line.bits = proposal_.bits;
      line.old_bits = cur.map.bits;
      line.at = proposal_.activation;
      line.logical = hop_.logical;
      line.active = desired.activeCount();
    } else if (!probe_.valid) {
      std::uint8_t ch = 0U;
      if (!estimator_.nextProbeChannel(now_ms, &ch)) return false;
      proposal_ = {amap::ProposalType::Probe, ++next_id_, cur.map_version, 0U,
                   hop_.logical + PR1_MAP_LEAD_FRAMES / 2U, ch};
      line.type = ctrl::OutType::PropProbe;
      line.id = proposal_.id;
      line.channel = ch;
      line.at = proposal_.activation;
      line.version = cur.map_version;
      line.logical = hop_.logical;
    } else {
      return false;
    }
    ++amap_.proposals;
    amap_.record({now_ms, static_cast<std::uint32_t>(hop_.logical),
                  static_cast<std::uint32_t>(proposal_.activation), proposal_.bits, proposal_.id,
                  proposal_.version, proposal_.channel, static_cast<std::uint8_t>(proposal_.type),
                  amap::EventKind::Proposed});
    emit(line);
    return true;
  }

  // Host relays the TX acknowledgement: "ACK id=<id> ok=<0|1>" (parsed on the control core).
  PR1_IRAM void rxControl(const ctrl::In& m) {
    const unsigned id = m.id, ok = m.ok;
    ctrl::Out out{};
    if (proposal_.type == amap::ProposalType::None || id != proposal_.id) {
      out.type = ctrl::OutType::CommitUnknown;
      out.id = m.id;
      out.tx_ok = m.ok;
      emit(out);
      return;
    }
    const bool in_time = hop_.locked && proposal_.activation > hop_.logical + PR1_MAP_GUARD_FRAMES;
    bool committed = false;
    if (ok != 0U && in_time) {
      if (proposal_.type == amap::ProposalType::Map) {
        afh::ChannelMap map{};
        map.bits = proposal_.bits;
        committed = scheduler_.stageMap(proposal_.version, map, proposal_.activation);
      } else {
        committed = estimator_.beginProbe(proposal_.channel, nowMs());
        if (committed) probe_ = {true, proposal_.id, proposal_.activation, proposal_.channel};
      }
    }
    if (committed) {
      ++amap_.commits;
    } else if (ok != 0U) {
      ++amap_.commit_late;  // TX staged but RX did not: the host must fail the run
    }
    amap_.record({nowMs(), static_cast<std::uint32_t>(hop_.logical),
                  static_cast<std::uint32_t>(proposal_.activation), proposal_.bits, proposal_.id,
                  proposal_.version, proposal_.channel, static_cast<std::uint8_t>(ok),
                  committed ? amap::EventKind::Committed : amap::EventKind::CommitLate});
    out.type = ctrl::OutType::Commit;
    out.id = proposal_.id;
    out.ok = committed ? 1U : 0U;
    out.tx_ok = m.ok;
    out.in_time = in_time ? 1U : 0U;
    out.logical = hop_.logical;
    proposal_.type = amap::ProposalType::None;
    if (!committed) next_proposal_ms_ = nowMs() + 1000U;
    emit(out);
  }

  // Host -> TX (two-phase), parsed on the control core:
  //   MAP id v old oldbits bits act   validate only
  //   PRB id ch at v                  validate only
  //   COMMIT id                       stage the validated candidate (only after the RX committed)
  //   ABORT id                        drop it
  PR1_IRAM void txControl(const ctrl::In& m) {
    const unsigned id = m.id;
    ctrl::Out out{};
    out.id = m.id;
    out.logical = hop_.logical;
    const char* reason = "parse";
    bool ok = false;
    if (m.type == ctrl::InType::Map) {
      const unsigned v = m.version, old_v = m.old_version;
      const std::uint64_t bits = m.bits, act = m.at;
      afh::ChannelMap map{};
      map.bits = bits;
      const auto& cur = scheduler_.current();
      if (cur.map_version != old_v || cur.map.bits != m.old_bits) {
        reason = "base_mismatch";
      } else if (v != static_cast<unsigned>(cur.map_version) + 1U || scheduler_.pending().valid) {
        reason = "version";
      } else if (!map.isValid()) {
        reason = "invalid";
      } else if (act <= hop_.logical + PR1_MAP_GUARD_FRAMES) {
        reason = "late";
      } else {
        candidate_ = {amap::ProposalType::Map, static_cast<std::uint16_t>(id), static_cast<std::uint16_t>(v),
                      bits, act, 0U};
        ok = true;
        reason = "validated";
      }
      out.type = ctrl::OutType::Ack;
    } else if (m.type == ctrl::InType::Probe) {
      const unsigned v = m.version, ch = m.channel;
      const std::uint64_t act = m.at;
      if (probe_.valid) {
        reason = "busy";
      } else if (ch >= afh::kChannelCount || act <= hop_.logical + PR1_MAP_GUARD_FRAMES / 2U) {
        reason = "late";
      } else {
        candidate_ = {amap::ProposalType::Probe, static_cast<std::uint16_t>(id), static_cast<std::uint16_t>(v),
                      0U, act, static_cast<std::uint8_t>(ch)};
        ok = true;
        reason = "validated";
      }
      out.type = ctrl::OutType::Ack;
    } else if (m.type == ctrl::InType::Commit) {
      if (candidate_.type == amap::ProposalType::None || candidate_.id != id) {
        reason = "unknown_id";
      } else if (candidate_.type == amap::ProposalType::Map) {
        afh::ChannelMap map{};
        map.bits = candidate_.bits;
        ok = candidate_.activation > hop_.logical &&
             scheduler_.stageMap(candidate_.version, map, candidate_.activation);
        reason = ok ? "staged" : "stage_failed";
      } else {
        ok = !probe_.valid && candidate_.activation > hop_.logical;
        if (ok) probe_ = {true, candidate_.id, candidate_.activation, candidate_.channel};
        reason = ok ? "probe_reserved" : "probe_late";
      }
      amap_.record({nowMs(), static_cast<std::uint32_t>(hop_.logical),
                    static_cast<std::uint32_t>(candidate_.activation), candidate_.bits, candidate_.id,
                    candidate_.version, candidate_.channel, static_cast<std::uint8_t>(candidate_.type),
                    ok ? amap::EventKind::TxStaged : amap::EventKind::TxRejected});
      ok ? ++amap_.commits : ++amap_.tx_rejects;
      candidate_.type = amap::ProposalType::None;
      out.type = ctrl::OutType::Staged;
    } else if (m.type == ctrl::InType::Abort) {
      const bool had = candidate_.type != amap::ProposalType::None && candidate_.id == id;
      if (had) candidate_.type = amap::ProposalType::None;
      ok = had;
      out.type = ctrl::OutType::Aborted;
    } else {
      return;
    }
    out.ok = ok ? 1U : 0U;
    out.reason = reason;
    emit(out);
  }

  RadioPort& radio_;
  RuntimeRole role_;
  FixedFlrcProfile profile_;
  std::uint16_t stream_id_ = 1U;
  std::uint16_t tx_sequence_ = 0U;
  std::uint32_t next_tx_allowed_us_ = 0U;
  pr1::sequence::SequenceUnwrapper sequence_unwrapper_{};
  LiveMetrics metrics_{};
  afh::Scheduler scheduler_{afhrt::staticScheduleConfig()};
  afhrt::HopTelemetry hop_{};
  bool hop_ever_heard_ = false;
  std::uint64_t last_rx_logical_ = 0U;
  std::uint32_t last_rx_irq_us_ = 0U;
  bool last_rx_valid_ = false;
  std::uint32_t hop_deadline_us_ = 0U;
  bool hop_deadline_valid_ = false;
  bool hop_acquired_ = false;
  // Gate C state (unused unless PR1_ENABLE_ADAPTIVE_MAP=1).
  quality::Estimator estimator_{};
  amap::Telemetry amap_{};
  amap::OutcomeQueue<64> outcomes_{};
  amap::Proposal proposal_{};
  amap::ProbeSlot probe_{};
  std::uint16_t next_id_ = 0U;
  afh::ScheduleConfig prev_config_{};
  std::uint64_t prev_activation_ = 0U;
  bool prev_valid_ = false;
  std::uint64_t last_probe_at_ = ~0ULL;
  std::uint8_t last_probe_channel_ = 0U;
  CtrlSink sink_ = nullptr;
  CtrlSource source_ = nullptr;
  amap::Proposal candidate_{};            // TX: validated, not yet committed
  std::uint32_t next_proposal_ms_ = 0U;   // RX: proposal backoff
  bool acq_anchor_valid_ = false;
  std::uint32_t tx_min_us_ = ~0U;
  bool diag_rx_valid_ = false;
  std::atomic<bool> ctrl_window_open_{true};
  std::uint32_t diag_ready_us_ = 0U;
  std::uint64_t diag_cold_logical_ = ~0ULL;
  std::uint32_t diag_irq_to_spi_ = 0U;
  std::uint32_t diag_spi_ = 0U;
  std::uint32_t diag_proc_ = 0U;
  std::uint32_t diag_rearm_ = 0U;
  std::uint64_t diag_ready_logical_ = 0U;
  std::uint64_t diag_rx_logical_ = 0U;
  std::uint32_t diag_rx_irq_us_ = 0U;
  std::uint16_t acq_raw_ = 0U;
  std::uint32_t acq_irq_us_ = 0U;
  bool deferred_check_valid_ = false;
  std::uint64_t deferred_check_logical_ = 0U;
  std::uint8_t deferred_check_channel_ = 0U;
  std::uint32_t resync_slot_ = 0U;
  PayloadSource payload_source_ = nullptr;
  PayloadSink payload_sink_ = nullptr;
  std::array<std::uint8_t, pr1::kRadioPayloadMaxBytes> tx_buffer_{};
  std::array<std::uint8_t, pr1::kRadioPayloadMaxBytes> rx_buffer_{};
  volatile bool rx_pending_ = false;
  volatile std::uint32_t rx_irq_timestamp_us_ = 0U;
  bool initialized_ = false;
};

}  // namespace pr1::runtime
