#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "../../common/pr1_packet.hpp"
#include "../../common/pr1_sequence.hpp"
#include "pr1_live_metrics.hpp"
#include "pr1_live_profile.hpp"
#include "pr1_radio_port.hpp"

namespace pr1::runtime {

class FixedLinkRuntime {
 public:
  FixedLinkRuntime(RadioPort& radio,
                   RuntimeRole role,
                   FixedFlrcProfile profile = kFixedFlrcProfile,
                   std::uint16_t stream_id = 1U)
      : radio_(radio), role_(role), profile_(profile), stream_id_(stream_id) {}

  bool begin() {
    if (role_ == RuntimeRole::Safe || !radio_.beginFixedFlrc(profile_)) return false;

    if (role_ == RuntimeRole::Rx) {
      radio_.setRxIrqHandler(&FixedLinkRuntime::rxIrqThunk, this);
      if (!radio_.startReceive()) return false;
    } else {
      next_tx_due_us_ = radio_.nowMicros();
    }

    initialized_ = true;
    return true;
  }

  void tick(std::uint32_t now_us) {
    if (!initialized_) return;
    if (role_ == RuntimeRole::Tx) {
      serviceTx(now_us);
    } else if (role_ == RuntimeRole::Rx && rx_pending_) {
      serviceRx();
    }
  }

  const LiveMetrics& metrics() const { return metrics_; }
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
    if (!deadlineReached(now_us, next_tx_due_us_)) return;

    const std::uint32_t lateness_us = now_us - next_tx_due_us_;
    if (lateness_us >= profile_.tx_period_us) {
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

    metrics_.onTxQueued(radio_.nowMicros(), tx_sequence_);
    metrics_.onTxStart(radio_.nowMicros(), tx_sequence_);
    const bool sent = radio_.transmit(tx_buffer_.data(), packet_len);
    if (sent) {
      metrics_.onTxDone(radio_.nowMicros(), tx_sequence_);
      tx_sequence_ = static_cast<std::uint16_t>(tx_sequence_ + 1U);
    }

    // Preserve cadence without trying to burst-send missed frames.
    const std::uint32_t periods_to_advance =
        profile_.tx_period_us == 0U ? 1U : (lateness_us / profile_.tx_period_us) + 1U;
    next_tx_due_us_ += periods_to_advance * profile_.tx_period_us;
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
        metrics_.onRxPacket(packet_done_us, decoded.header.sequence, radio_.rssiDbm());
        observeSequence(decoded.header.sequence);
      }
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

  RadioPort& radio_;
  RuntimeRole role_;
  FixedFlrcProfile profile_;
  std::uint16_t stream_id_ = 1U;
  std::uint16_t tx_sequence_ = 0U;
  std::uint32_t next_tx_due_us_ = 0U;
  pr1::sequence::SequenceUnwrapper sequence_unwrapper_{};
  LiveMetrics metrics_{};
  std::array<std::uint8_t, pr1::kRadioPayloadMaxBytes> tx_buffer_{};
  std::array<std::uint8_t, pr1::kRadioPayloadMaxBytes> rx_buffer_{};
  volatile bool rx_pending_ = false;
  volatile std::uint32_t rx_irq_timestamp_us_ = 0U;
  bool initialized_ = false;
};

}  // namespace pr1::runtime
