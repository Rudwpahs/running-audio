#pragma once

#include <cstdint>
#include <limits>

#include "../../common/pr1_instrumentation.hpp"
#include "../../common/pr1_telemetry.hpp"

namespace pr1::runtime {

class LiveMetrics {
 public:
  void onTxQueued(std::uint32_t timestamp_us, std::uint32_t sequence) {
    scheduler_observed_ = true;
    push(pr1::instrumentation::Event::RadioTxEnqueue, timestamp_us, sequence, 0);
  }

  void onTxStart(std::uint32_t timestamp_us, std::uint32_t sequence) {
    push(pr1::instrumentation::Event::Sx1280TxStart, timestamp_us, sequence, 0);
  }

  void onTxDone(std::uint32_t timestamp_us, std::uint32_t sequence) {
    push(pr1::instrumentation::Event::Sx1280TxDone, timestamp_us, sequence, 0);
  }

  void onRxIrq(std::uint32_t timestamp_us, std::uint32_t sequence) {
    last_rx_irq_us_ = timestamp_us;
    have_rx_irq_ = true;
    push(pr1::instrumentation::Event::DioRxDone, timestamp_us, sequence, 0);
    push(pr1::instrumentation::Event::IsrEnter, timestamp_us, sequence, 0);
  }

  void onSpiStart(std::uint32_t timestamp_us, std::uint32_t sequence) {
    if (have_rx_irq_) {
      irq_to_spi_.observe(elapsed(last_rx_irq_us_, timestamp_us));
    }
    last_spi_start_us_ = timestamp_us;
    have_spi_start_ = true;
    push(pr1::instrumentation::Event::SpiReadStart, timestamp_us, sequence, 0);
  }

  void onSpiEnd(std::uint32_t timestamp_us, std::uint32_t sequence) {
    if (have_spi_start_) {
      spi_duration_.observe(elapsed(last_spi_start_us_, timestamp_us));
    }
    last_spi_end_us_ = timestamp_us;
    have_spi_end_ = true;
    push(pr1::instrumentation::Event::SpiReadEnd, timestamp_us, sequence, 0);
  }

  void onRxPacket(std::uint32_t timestamp_us, std::uint32_t sequence, std::int16_t rssi_dbm) {
    if (have_spi_start_) {
      rx_processing_.observe(elapsed(last_spi_start_us_, timestamp_us));
    }
    ++counters_.crc_good;
    rf_outcome_observed_ = true;
    rssi_available_ = true;
    last_rssi_dbm_ = rssi_dbm;
    push(pr1::instrumentation::Event::RxPacketOk, timestamp_us, sequence, rssi_dbm);
  }

  void onRxCrcFail(std::uint32_t timestamp_us, std::uint32_t sequence) {
    ++counters_.crc_bad;
    rf_outcome_observed_ = true;
    push(pr1::instrumentation::Event::RxCrcFail, timestamp_us, sequence, 0);
  }

  void onRxRearmStart(std::uint32_t timestamp_us, std::uint32_t sequence) {
    if (have_spi_end_) {
      spi_end_to_rearm_start_.observe(elapsed(last_spi_end_us_, timestamp_us));
    }
    last_rearm_start_us_ = timestamp_us;
    have_rearm_start_ = true;
    push(pr1::instrumentation::Event::RxRearmStart, timestamp_us, sequence, 0);
  }

  void onRxRearmDone(std::uint32_t timestamp_us, std::uint32_t sequence) {
    if (have_rearm_start_) {
      rx_rearm_.observe(elapsed(last_rearm_start_us_, timestamp_us));
    }
    if (have_rx_irq_) {
      irq_to_rx_ready_.observe(elapsed(last_rx_irq_us_, timestamp_us));
    }
    push(pr1::instrumentation::Event::RxRearmDone, timestamp_us, sequence, 0);
  }

  void onQueueDepth(std::uint16_t depth, std::uint32_t timestamp_us, std::uint32_t sequence) {
    queue_observed_ = true;
    current_queue_depth_ = depth;
    counters_.observeQueueDepth(depth);
    const auto traced_depth =
        depth > static_cast<std::uint16_t>(std::numeric_limits<std::int16_t>::max())
            ? std::numeric_limits<std::int16_t>::max()
            : static_cast<std::int16_t>(depth);
    push(pr1::instrumentation::Event::QueueDepth, timestamp_us, sequence, traced_depth);
  }

  void onMissing(std::uint32_t count) {
    missing_observed_ = true;
    counters_.missing += count;
  }

  void onSchedulerMiss(std::uint32_t timestamp_us, std::uint32_t sequence) {
    scheduler_observed_ = true;
    ++counters_.scheduler_misses;
    push(pr1::instrumentation::Event::SchedulerMiss, timestamp_us, sequence, 0);
  }

  pr1::telemetry::Snapshot snapshot() const {
    pr1::telemetry::Snapshot out{};
    out.state = pr1::telemetry::DeviceState::Ready;
    out.capability_mask =
        pr1::telemetry::capabilityMask(pr1::telemetry::Capability::RfStats,
                                       pr1::telemetry::Capability::TimingTrace);
    out.counters = counters_;
    out.trace_overwrites = trace_.overwrites();

    if (rssi_available_) out.rssi_dbm = {true, last_rssi_dbm_};
    if (rf_outcome_observed_) {
      out.crc_good = {true, counters_.crc_good};
      out.crc_bad = {true, counters_.crc_bad};
    }
    if (missing_observed_) out.missing = {true, counters_.missing};
    if (queue_observed_) {
      out.queue_depth = {true, current_queue_depth_};
      out.max_queue_depth = {true, counters_.max_queue_depth};
    }
    if (scheduler_observed_) {
      out.scheduler_misses = {true, counters_.scheduler_misses};
    }
    if (irq_to_spi_.size() > 0U) out.irq_to_spi_us = {true, irq_to_spi_.percentile(99)};
    if (spi_duration_.size() > 0U) out.spi_duration_us = {true, spi_duration_.percentile(99)};
    if (rx_processing_.size() > 0U) {
      out.rx_processing_us = {true, rx_processing_.percentile(99)};
    }
    if (spi_end_to_rearm_start_.size() > 0U) {
      out.spi_end_to_rearm_start_us = {true, spi_end_to_rearm_start_.percentile(99)};
    }
    if (rx_rearm_.size() > 0U) out.rx_rearm_us = {true, rx_rearm_.percentile(99)};
    if (irq_to_rx_ready_.size() > 0U) {
      out.irq_to_rx_ready_us = {true, irq_to_rx_ready_.percentile(99)};
    }
    return out;
  }

  std::uint32_t traceOverwrites() const { return trace_.overwrites(); }

 private:
  static constexpr std::uint32_t elapsed(std::uint32_t start, std::uint32_t end) {
    return end - start;
  }

  void push(pr1::instrumentation::Event event,
            std::uint32_t timestamp_us,
            std::uint32_t sequence,
            std::int16_t value) {
    trace_.push({event, timestamp_us, sequence, value});
  }

  pr1::instrumentation::Counters counters_{};
  pr1::instrumentation::TraceRing<128> trace_{};
  pr1::instrumentation::DurationWindow<64> irq_to_spi_{};
  pr1::instrumentation::DurationWindow<64> spi_duration_{};
  pr1::instrumentation::DurationWindow<64> rx_processing_{};
  pr1::instrumentation::DurationWindow<64> spi_end_to_rearm_start_{};
  pr1::instrumentation::DurationWindow<64> rx_rearm_{};
  pr1::instrumentation::DurationWindow<64> irq_to_rx_ready_{};

  std::uint32_t last_rx_irq_us_ = 0;
  std::uint32_t last_spi_start_us_ = 0;
  std::uint32_t last_spi_end_us_ = 0;
  std::uint32_t last_rearm_start_us_ = 0;
  std::uint16_t current_queue_depth_ = 0;
  std::int16_t last_rssi_dbm_ = 0;

  bool have_rx_irq_ = false;
  bool have_spi_start_ = false;
  bool have_spi_end_ = false;
  bool have_rearm_start_ = false;
  bool rssi_available_ = false;
  bool rf_outcome_observed_ = false;
  bool missing_observed_ = false;
  bool queue_observed_ = false;
  bool scheduler_observed_ = false;
};

}  // namespace pr1::runtime
