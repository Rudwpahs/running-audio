#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "../../common/pr1_packet.hpp"
#include "../../common/pr1_sequence.hpp"
#include "pr1_afh_runtime.hpp"
#include "pr1_live_metrics.hpp"
#include "pr1_live_profile.hpp"
#include "pr1_radio_port.hpp"

namespace pr1::runtime {

class FixedLinkRuntime {
 public:
  static constexpr bool kAfhEnabled = afh::kEnabledByDefault;

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
      tx_channel = scheduler_.channelForSequence(hop_.logical);
      hop_.hop_compute_us.observe(radio_.nowMicros() - compute_start_us);
      if (!retune(tx_channel)) {
        next_tx_allowed_us_ = radio_.nowMicros() + profile_.tx_gap_us;
        return;
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
      metrics_.onRxCrcFail(spi_end_us, label_sequence);
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
        metrics_.onRxPacket(packet_done_us, decoded.header.sequence, rssi);
        observeSequence(decoded.header.sequence);
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

  bool retune(std::uint8_t channel) {
    const std::uint32_t start_us = radio_.nowMicros();
    const bool ok = radio_.setFrequencyHz(afh::frequencyHz(channel));
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

  // Margin after the predicted RX-done time before declaring the frame lost.
  // Must stay well below the on-air idle time so the retune lands before the
  // next preamble (P - airtime ~1.9 ms at 150 us gap).
  static std::uint32_t lossMargin(std::uint32_t period) {
    const std::uint32_t m = period / 8U;
    return m < 150U ? 150U : (m > 400U ? 400U : m);
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
    const std::uint8_t channel = scheduler_.channelForSequence(logical);
    hop_.hop_compute_us.observe(radio_.nowMicros() - start_us);
    return channel;
  }

  // Called after every receive-complete, with the radio in standby (readData).
  void followAfterRx(RadioReadResult result, bool decoded, std::uint16_t raw, std::int16_t rssi,
                     std::uint32_t irq_us) {
    const std::uint8_t heard_channel = hop_.current_channel;
    if (decoded) {
      const std::uint64_t logical = hopLogicalFor(raw);
      if (hop_.locked && logical < hop_.logical) {
        // Duplicate / older frame: keep channel and deadline unchanged.
        hop_.record({irq_us, static_cast<std::uint32_t>(logical), raw, heard_channel,
                     afhrt::HopEventKind::RxOther, rssi});
        return;
      }
      // Tuned to the expected frame's channel by construction when locked and on time;
      // otherwise check the heard frame against the schedule explicitly.
      const bool on_expected = hop_.locked && logical == hop_.logical;
      const std::uint8_t predicted = on_expected ? heard_channel : timedChannelFor(logical);
      if (predicted == heard_channel) {
        ++hop_.schedule_agree;
      } else {
        ++hop_.schedule_disagree;
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
      if (!hop_.locked) {
        ++hop_.locks;
        hop_.record({irq_us, static_cast<std::uint32_t>(logical), raw, heard_channel,
                     afhrt::HopEventKind::Lock, rssi});
      }
      hop_.record({irq_us, static_cast<std::uint32_t>(logical), raw, heard_channel,
                   afhrt::HopEventKind::RxOk, rssi});
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
      const std::uint8_t next = timedChannelFor(hop_.logical);
      hop_.record({now_us, static_cast<std::uint32_t>(hop_.logical),
                   static_cast<std::uint16_t>(frames), next, afhrt::HopEventKind::TimeoutAdvance, 0});
      retune(next);
    }
    if (!radio_.startReceive()) initialized_ = false;
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
  std::uint32_t resync_slot_ = 0U;
  std::array<std::uint8_t, pr1::kRadioPayloadMaxBytes> tx_buffer_{};
  std::array<std::uint8_t, pr1::kRadioPayloadMaxBytes> rx_buffer_{};
  volatile bool rx_pending_ = false;
  volatile std::uint32_t rx_irq_timestamp_us_ = 0U;
  bool initialized_ = false;
};

}  // namespace pr1::runtime
