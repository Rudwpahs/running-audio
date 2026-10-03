#pragma once

#include <array>
#include <cinttypes>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>

#include "../../common/pr1_packet.hpp"
#include "../../common/pr1_sequence.hpp"
#include "pr1_adaptive_map_runtime.hpp"
#include "pr1_afh_runtime.hpp"
#include "pr1_live_metrics.hpp"
#include "pr1_live_profile.hpp"
#include "pr1_radio_port.hpp"

namespace pr1::runtime {

class FixedLinkRuntime {
 public:
  static constexpr bool kAfhEnabled = afh::kEnabledByDefault;
  static constexpr bool kAdaptive = kAfhEnabled && amap::kEnabled;
  using LineSink = bool (*)(const char* line);

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
      serviceTx(now_us);
    } else if (role_ == RuntimeRole::Rx) {
      if (rx_pending_) {
        serviceRx();
      } else if constexpr (kAfhEnabled) {
        serviceHopTimeout(now_us);
      }
    }
  }

  const LiveMetrics& metrics() const { return metrics_; }
  const afhrt::HopTelemetry& hopTelemetry() const { return hop_; }
  const afh::Scheduler& hopScheduler() const { return scheduler_; }
  const amap::Telemetry& mapTelemetry() const { return amap_; }
  const quality::Estimator& estimator() const { return estimator_; }
  const amap::ProbeSlot& probeSlot() const { return probe_; }
  void setLineSink(LineSink sink) { sink_ = sink; }

  // Gate C bench control plane (USB/host relay). Lines are short ASCII commands.
  void onControlLine(const char* line) {
    if constexpr (kAdaptive) {
      if (role_ == RuntimeRole::Tx) {
        const std::uint32_t start_us = radio_.nowMicros();
        txControl(line);
        amap_.ctrl_us.observe(radio_.nowMicros() - start_us);
      } else if (role_ == RuntimeRole::Rx) {
        // Never handle on the RX loop directly: defer to serviceQuality's safe window.
        std::strncpy(rx_ctrl_line_, line, sizeof(rx_ctrl_line_) - 1U);
        rx_ctrl_line_[sizeof(rx_ctrl_line_) - 1U] = '\0';
        rx_ctrl_pending_ = true;
      }
    } else {
      (void)line;
    }
  }
  bool initialized() const { return initialized_; }

 private:
  static void rxIrqThunk(void* context, std::uint32_t timestamp_us) {
    if (context == nullptr) return;
    auto* self = static_cast<FixedLinkRuntime*>(context);
    // Single-producer ISR contract: RX is not re-armed until the pending event
    // is serviced in tick(), so at most one receive-complete event is pending.
    self->rx_irq_timestamp_us_ = timestamp_us;
    self->rx_pending_ = true;
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
    metrics_.onTxStart(radio_.nowMicros(), tx_sequence_);
    const bool sent = radio_.transmit(tx_buffer_.data(), packet_len);
    const std::uint32_t tx_attempt_done_us = radio_.nowMicros();
    if constexpr (kAfhEnabled) {
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
    metrics_.onQueueDepth(0U, rearm_done_us, label_sequence);
    if (!rearmed) initialized_ = false;
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
  std::uint8_t channelFor(std::uint64_t logical) {
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
  std::uint8_t channelForPast(std::uint64_t logical) const {
    if (prev_valid_ && logical < prev_activation_) {
      const afh::Scheduler previous(prev_config_);
      return previous.channelForSequence(logical);
    }
    return scheduler_.channelForSequence(logical);
  }

  std::uint32_t nowMs() const { return radio_.nowMicros() / 1000U; }

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

  // RX idle: if the predicted frame did not arrive, advance on the TX cadence.
  void serviceHopTimeout(std::uint32_t now_us) {
    if constexpr (kAdaptive) {
      if (!deadlineNear(now_us) && serviceQuality()) return;
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

  void queueOutcome(std::uint64_t logical, std::uint8_t channel, amap::OutcomeKind kind,
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

  std::uint64_t logicalFromLo(std::uint32_t lo) const {
    // Outcomes are at most a few frames old: rebuild the 64-bit frame near hop_.logical.
    const std::uint64_t ref = hop_.logical;
    const auto diff = static_cast<std::int32_t>(lo - static_cast<std::uint32_t>(ref));
    return ref + static_cast<std::int64_t>(diff);
  }

  // Returns true if it did any work (one outcome or one proposal per call).
  bool serviceQuality() {
    if (rx_ctrl_pending_) {
      rx_ctrl_pending_ = false;
      const std::uint32_t start_us = radio_.nowMicros();
      rxControl(rx_ctrl_line_);
      amap_.ctrl_us.observe(radio_.nowMicros() - start_us);
      return true;
    }
    amap::Outcome o{};
    if (outcomes_.pop(&o)) {
      const std::uint64_t logical = logicalFromLo(o.logical_lo);
      const std::uint8_t ch = o.channel == 255U ? channelForPast(logical) : o.channel;
      if (ch >= afh::kChannelCount) return true;
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
        return true;
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
      return true;
    }
    return maybePropose();
  }

  void emit(const char* line) {
    if (sink_ == nullptr || !sink_(line)) ++amap_.emit_dropped;
  }

  void abandonProposal(const char* reason) {
    char out[120];
    std::snprintf(out, sizeof(out), "PR1COMMIT id=%u ok=0 tx_ok=0 in_time=0 reason=%s rx_logical=%" PRIu64,
                  static_cast<unsigned>(proposal_.id), reason, hop_.logical);
    proposal_.type = amap::ProposalType::None;
    next_proposal_ms_ = nowMs() + 1000U;  // back off after any failed proposal
    emit(out);
  }

  bool maybePropose() {
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
    char line[200];
    if (desired.bits != cur.map.bits && desired.isValid()) {
      proposal_ = {amap::ProposalType::Map, ++next_id_, static_cast<std::uint16_t>(cur.map_version + 1U),
                   desired.bits, hop_.logical + PR1_MAP_LEAD_FRAMES, 0U};
      std::snprintf(line, sizeof(line),
                    "PR1PROP id=%u type=map v=%u old=%u bits=%010" PRIx64 " oldbits=%010" PRIx64
                    " act=%" PRIu64 " rx_logical=%" PRIu64 " active=%u",
                    static_cast<unsigned>(proposal_.id), static_cast<unsigned>(proposal_.version),
                    static_cast<unsigned>(cur.map_version), proposal_.bits, cur.map.bits,
                    proposal_.activation, hop_.logical, static_cast<unsigned>(desired.activeCount()));
    } else if (!probe_.valid) {
      std::uint8_t ch = 0U;
      if (!estimator_.nextProbeChannel(now_ms, &ch)) return false;
      proposal_ = {amap::ProposalType::Probe, ++next_id_, cur.map_version, 0U,
                   hop_.logical + PR1_MAP_LEAD_FRAMES / 2U, ch};
      std::snprintf(line, sizeof(line),
                    "PR1PROP id=%u type=probe ch=%u at=%" PRIu64 " v=%u rx_logical=%" PRIu64,
                    static_cast<unsigned>(proposal_.id), static_cast<unsigned>(ch), proposal_.activation,
                    static_cast<unsigned>(cur.map_version), hop_.logical);
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

  // Host relays the TX acknowledgement: "ACK id=<id> ok=<0|1>".
  void rxControl(const char* line) {
    unsigned long long id_ = 0ULL, ok_ = 0ULL;
    char out[120];
    if (!starts(line, "ACK") || !kv(line, "id", 10, &id_) || !kv(line, "ok", 10, &ok_)) return;
    const unsigned id = static_cast<unsigned>(id_), ok = static_cast<unsigned>(ok_);
    if (proposal_.type == amap::ProposalType::None || id != proposal_.id) {
      std::snprintf(out, sizeof(out), "PR1COMMIT id=%u ok=0 tx_ok=%u in_time=0 reason=unknown_id", id, ok);
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
    std::snprintf(out, sizeof(out), "PR1COMMIT id=%u ok=%u tx_ok=%u in_time=%u rx_logical=%" PRIu64,
                  static_cast<unsigned>(proposal_.id), committed ? 1U : 0U, ok, in_time ? 1U : 0U,
                  hop_.logical);
    proposal_.type = amap::ProposalType::None;
    if (!committed) next_proposal_ms_ = nowMs() + 1000U;
    emit(out);
  }

  // Minimal "key=value" reader for control lines (no sscanf on the radio loop).
  static bool kv(const char* line, const char* key, int base, unsigned long long* out) {
    const std::size_t klen = std::strlen(key);
    for (const char* p = line; (p = std::strstr(p, key)) != nullptr; p += klen) {
      if ((p == line || p[-1] == ' ') && p[klen] == '=') {
        char* end = nullptr;
        *out = std::strtoull(p + klen + 1, &end, base);
        return end != p + klen + 1;
      }
    }
    return false;
  }
  static bool starts(const char* line, const char* word) {
    return std::strncmp(line, word, std::strlen(word)) == 0 && line[std::strlen(word)] == ' ';
  }

  // Host -> TX (two-phase):
  //   "MAP id=<id> v=<ver> old=<ver> oldbits=<hex> bits=<hex> act=<frame>"  validate only
  //   "PRB id=<id> ch=<c> at=<frame> v=<ver>"                                validate only
  //   "COMMIT id=<id>"  stage the validated candidate (only after the RX committed)
  //   "ABORT id=<id>"   drop it
  void txControl(const char* line) {
    unsigned long long id_ = 0ULL, v_ = 0ULL, old_v_ = 0ULL, ch_ = 0ULL;
    unsigned long long bits = 0ULL, old_bits = 0ULL, act = 0ULL;
    const bool has_id = kv(line, "id", 10, &id_);
    const unsigned id = static_cast<unsigned>(id_);
    char out[140];
    const char* reason = "parse";
    bool ok = false;
    if (starts(line, "MAP") && has_id && kv(line, "v", 10, &v_) && kv(line, "old", 10, &old_v_) &&
        kv(line, "oldbits", 16, &old_bits) && kv(line, "bits", 16, &bits) && kv(line, "act", 10, &act)) {
      const unsigned v = static_cast<unsigned>(v_), old_v = static_cast<unsigned>(old_v_);
      afh::ChannelMap map{};
      map.bits = bits;
      const auto& cur = scheduler_.current();
      if (cur.map_version != old_v || cur.map.bits != old_bits) {
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
      std::snprintf(out, sizeof(out), "PR1ACK id=%u ok=%u reason=%s tx_logical=%" PRIu64, id, ok ? 1U : 0U,
                    reason, hop_.logical);
    } else if (starts(line, "PRB") && has_id && kv(line, "ch", 10, &ch_) && kv(line, "at", 10, &act) &&
               kv(line, "v", 10, &v_)) {
      const unsigned v = static_cast<unsigned>(v_), ch = static_cast<unsigned>(ch_);
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
      std::snprintf(out, sizeof(out), "PR1ACK id=%u ok=%u reason=%s tx_logical=%" PRIu64, id, ok ? 1U : 0U,
                    reason, hop_.logical);
    } else if (starts(line, "COMMIT") && has_id) {
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
      std::snprintf(out, sizeof(out), "PR1STAGED id=%u ok=%u reason=%s tx_logical=%" PRIu64, id, ok ? 1U : 0U,
                    reason, hop_.logical);
    } else if (starts(line, "ABORT") && has_id) {
      const bool had = candidate_.type != amap::ProposalType::None && candidate_.id == id;
      if (had) candidate_.type = amap::ProposalType::None;
      std::snprintf(out, sizeof(out), "PR1ABORTED id=%u had=%u", id, had ? 1U : 0U);
    } else {
      return;
    }
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
  LineSink sink_ = nullptr;
  amap::Proposal candidate_{};            // TX: validated, not yet committed
  std::uint32_t next_proposal_ms_ = 0U;   // RX: proposal backoff
  char rx_ctrl_line_[160] = {};
  volatile bool rx_ctrl_pending_ = false;
  bool acq_anchor_valid_ = false;
  std::uint16_t acq_raw_ = 0U;
  std::uint32_t acq_irq_us_ = 0U;
  bool deferred_check_valid_ = false;
  std::uint64_t deferred_check_logical_ = 0U;
  std::uint8_t deferred_check_channel_ = 0U;
  std::uint32_t resync_slot_ = 0U;
  std::array<std::uint8_t, pr1::kRadioPayloadMaxBytes> tx_buffer_{};
  std::array<std::uint8_t, pr1::kRadioPayloadMaxBytes> rx_buffer_{};
  volatile bool rx_pending_ = false;
  volatile std::uint32_t rx_irq_timestamp_us_ = 0U;
  bool initialized_ = false;
};

}  // namespace pr1::runtime
