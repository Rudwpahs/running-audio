#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace pr1::instrumentation {

enum class Event : std::uint8_t {
  AudioCaptureDone,
  OpusEncodeStart,
  OpusEncodeEnd,
  RadioTxEnqueue,
  Sx1280TxStart,
  Sx1280TxDone,
  DioRxDone,
  IsrEnter,
  SpiReadStart,
  SpiReadEnd,
  FecRecovered,
  NackSent,
  RetransmitSent,
  PlcUsed,
  AudioPlayed,
  SchedulerMiss,
};

struct TraceEntry {
  Event event = Event::AudioCaptureDone;
  std::uint32_t timestamp_us = 0;
  std::uint32_t sequence = 0;
  std::int16_t value = 0;
};

template <std::size_t Capacity>
class TraceRing {
 public:
  static_assert(Capacity > 0, "Trace ring must have non-zero capacity");

  void push(const TraceEntry& entry) {
    entries_[write_index_] = entry;
    write_index_ = (write_index_ + 1U) % Capacity;
    if (size_ < Capacity) {
      ++size_;
    } else {
      ++overwrites_;
    }
  }

  std::size_t size() const { return size_; }
  std::uint32_t overwrites() const { return overwrites_; }

  bool atOldest(std::size_t index, TraceEntry* out) const {
    if (out == nullptr || index >= size_) return false;
    const std::size_t oldest = size_ == Capacity ? write_index_ : 0U;
    *out = entries_[(oldest + index) % Capacity];
    return true;
  }

 private:
  std::array<TraceEntry, Capacity> entries_{};
  std::size_t write_index_ = 0;
  std::size_t size_ = 0;
  std::uint32_t overwrites_ = 0;
};

template <std::size_t Capacity>
class DurationWindow {
 public:
  static_assert(Capacity > 0, "Duration window must have non-zero capacity");

  void observe(std::uint32_t value_us) {
    values_[write_index_] = value_us;
    write_index_ = (write_index_ + 1U) % Capacity;
    if (size_ < Capacity) ++size_;
    if (value_us > max_us_) max_us_ = value_us;
  }

  std::size_t size() const { return size_; }
  std::uint32_t maxUs() const { return max_us_; }

  std::uint32_t percentile(unsigned pct) const {
    if (size_ == 0) return 0;
    if (pct > 100U) pct = 100U;
    std::array<std::uint32_t, Capacity> copy{};
    for (std::size_t i = 0; i < size_; ++i) copy[i] = values_[i];
    for (std::size_t i = 1; i < size_; ++i) {
      const std::uint32_t v = copy[i];
      std::size_t j = i;
      while (j > 0 && copy[j - 1] > v) {
        copy[j] = copy[j - 1];
        --j;
      }
      copy[j] = v;
    }
    const std::size_t rank =
        (static_cast<std::size_t>(pct) * (size_ - 1U) + 99U) / 100U;
    return copy[rank];
  }

 private:
  std::array<std::uint32_t, Capacity> values_{};
  std::size_t write_index_ = 0;
  std::size_t size_ = 0;
  std::uint32_t max_us_ = 0;
};

struct Counters {
  std::uint32_t crc_good = 0;
  std::uint32_t crc_bad = 0;
  std::uint32_t missing = 0;
  std::uint32_t scheduler_misses = 0;
  std::uint32_t fec_recovered = 0;
  std::uint32_t nack_sent = 0;
  std::uint32_t retransmit_sent = 0;
  std::uint32_t plc_used = 0;
  std::uint32_t audio_played = 0;
  std::uint16_t max_queue_depth = 0;

  void observeQueueDepth(std::uint16_t depth) {
    if (depth > max_queue_depth) max_queue_depth = depth;
  }
};

}  // namespace pr1::instrumentation
