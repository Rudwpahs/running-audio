#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>

#include "pr1_afh.hpp"

namespace pr1::quality {

enum class ChannelState : std::uint8_t { Active, Suspect, Excluded, Probe };

struct Config {
  // PDR EWMAs: alpha = 1 / (2^shift). Defaults are 1/4 and 1/32.
  std::uint8_t alpha_fast_shift = 2;
  std::uint8_t alpha_slow_shift = 5;

  // State thresholds. Q15 represents [0, 1.0] as [0, 32767].
  std::uint16_t suspect_pdr_q15 = 30000;
  std::uint16_t exclude_pdr_q15 = 27853;
  std::uint16_t recover_pdr_q15 = 30720;
  std::uint8_t suspect_losses = 2;
  std::uint8_t exclude_losses = 4;
  std::uint8_t recover_successes = 3;

  // Protected re-exploration defaults from issue #26.
  std::uint32_t initial_probe_ms = 200;
  std::uint32_t max_probe_ms = 3200;
  std::uint8_t minimum_active_channels = 12;

  // Reprobe-score weights. Higher score is probed first among due channels.
  // They are runtime-configurable to allow hardware tuning later.
  std::uint16_t probe_age_weight = 2;
  std::uint16_t probe_history_weight = 3;
  std::uint16_t probe_neighbor_weight = 2;
  std::uint16_t probe_backoff_penalty = 250;
};

struct ChannelStats {
  std::uint16_t pdr_fast_q15 = 32767;
  std::uint16_t pdr_slow_q15 = 32767;
  std::uint8_t consecutive_losses = 0;
  std::uint8_t consecutive_successes = 0;
  std::uint32_t last_seen_ms = 0;
  std::uint32_t last_probe_ms = 0;
  std::uint8_t probe_failure_exp = 0;
  ChannelState state = ChannelState::Active;
  std::uint8_t probe_history_bits = 0;
  std::uint8_t probe_history_count = 0;
};

// Dedicated tiny packet for re-exploring an excluded channel. It deliberately
// carries no audio/FEC payload, so losing it cannot reduce audio repair budget.
constexpr std::uint8_t kMicroProbeMagic = 0xD3;
constexpr std::uint8_t kMicroProbeVersion = 1;
constexpr std::size_t kMicroProbeBytes = 8;

struct MicroProbe {
  std::uint8_t channel = 0;
  std::uint16_t map_version = 0;
  std::uint16_t token = 0;
};

inline bool encodeMicroProbe(const MicroProbe& probe,
                             std::array<std::uint8_t, kMicroProbeBytes>* out) {
  if (out == nullptr || probe.channel >= afh::kChannelCount) return false;
  (*out)[0] = kMicroProbeMagic;
  (*out)[1] = kMicroProbeVersion;
  (*out)[2] = probe.channel;
  (*out)[3] = 0;
  (*out)[4] = static_cast<std::uint8_t>(probe.map_version >> 8U);
  (*out)[5] = static_cast<std::uint8_t>(probe.map_version & 0xFFU);
  (*out)[6] = static_cast<std::uint8_t>(probe.token >> 8U);
  (*out)[7] = static_cast<std::uint8_t>(probe.token & 0xFFU);
  return true;
}

inline bool decodeMicroProbe(const std::uint8_t* data, std::size_t len,
                             MicroProbe* out) {
  if (data == nullptr || out == nullptr || len != kMicroProbeBytes ||
      data[0] != kMicroProbeMagic || data[1] != kMicroProbeVersion ||
      data[2] >= afh::kChannelCount) {
    return false;
  }
  out->channel = data[2];
  out->map_version = static_cast<std::uint16_t>(
      (static_cast<std::uint16_t>(data[4]) << 8U) | data[5]);
  out->token = static_cast<std::uint16_t>(
      (static_cast<std::uint16_t>(data[6]) << 8U) | data[7]);
  return true;
}

class Estimator {
 public:
  explicit Estimator(Config config = {}) : config_(config) {}

  const Config& config() const { return config_; }
  void setConfig(const Config& config) { config_ = config; }

  const ChannelStats& channel(std::uint8_t c) const {
    if (c < afh::kChannelCount) return channels_[c];
    static const ChannelStats invalid{};
    return invalid;
  }
  std::uint8_t activeCount() const { return active_count_; }

  void observeData(std::uint8_t c, bool success, std::uint32_t now_ms) {
    if (c >= afh::kChannelCount) return;
    auto& s = channels_[c];
    s.last_seen_ms = now_ms;
    ewma(s.pdr_fast_q15, success, config_.alpha_fast_shift);
    ewma(s.pdr_slow_q15, success, config_.alpha_slow_shift);

    if (success) {
      s.consecutive_losses = 0;
      if (s.consecutive_successes < 255U) ++s.consecutive_successes;
    } else {
      s.consecutive_successes = 0;
      if (s.consecutive_losses < 255U) ++s.consecutive_losses;
    }

    if (s.state == ChannelState::Active) {
      if (s.consecutive_losses >= config_.suspect_losses ||
          s.pdr_fast_q15 < config_.suspect_pdr_q15) {
        s.state = ChannelState::Suspect;
        s.consecutive_successes = 0;
      }
      return;
    }

    if (s.state != ChannelState::Suspect) return;

    const bool recovered = success &&
                           s.consecutive_successes >= config_.recover_successes &&
                           s.pdr_fast_q15 >= config_.recover_pdr_q15;
    if (recovered) {
      s.state = ChannelState::Active;
      return;
    }

    const bool should_exclude =
        s.consecutive_losses >= config_.exclude_losses ||
        s.pdr_fast_q15 < config_.exclude_pdr_q15;
    if (should_exclude && active_count_ > config_.minimum_active_channels) {
      excludeChannel(c, now_ms);
    }
  }

  bool probeDue(std::uint8_t c, std::uint32_t now_ms) const {
    if (c >= afh::kChannelCount) return false;
    const auto& s = channels_[c];
    if (s.state != ChannelState::Excluded && s.state != ChannelState::Probe) {
      return false;
    }
    return elapsedMs(now_ms, s.last_probe_ms) >= probeIntervalMs(s);
  }

  // Returns a relative priority only. It is intentionally simple/integer-only
  // so it can run cheaply on ESP32-S3 and be tuned from hardware data later.
  std::int32_t reprobeScore(std::uint8_t c, std::uint32_t now_ms) const {
    if (c >= afh::kChannelCount || !probeDue(c, now_ms)) {
      return std::numeric_limits<std::int32_t>::min();
    }
    const auto& s = channels_[c];
    const std::uint32_t age_ms = elapsedMs(now_ms, s.last_probe_ms);
    const std::uint32_t age_units = age_ms > 60000U ? 6000U : age_ms / 10U;
    const std::uint32_t history_permille =
        (static_cast<std::uint32_t>(s.pdr_slow_q15) * 1000U) / 32767U;
    const std::uint32_t neighbor_permille =
        (static_cast<std::uint32_t>(neighborQualityQ15(c)) * 1000U) / 32767U;

    std::int64_t score =
        static_cast<std::int64_t>(age_units) * config_.probe_age_weight +
        static_cast<std::int64_t>(history_permille) * config_.probe_history_weight +
        static_cast<std::int64_t>(neighbor_permille) * config_.probe_neighbor_weight -
        static_cast<std::int64_t>(s.probe_failure_exp) * config_.probe_backoff_penalty;
    if (score > std::numeric_limits<std::int32_t>::max()) {
      score = std::numeric_limits<std::int32_t>::max();
    }
    if (score < std::numeric_limits<std::int32_t>::min()) {
      score = std::numeric_limits<std::int32_t>::min();
    }
    return static_cast<std::int32_t>(score);
  }

  bool nextProbeChannel(std::uint32_t now_ms, std::uint8_t* out_channel) const {
    if (out_channel == nullptr) return false;
    bool found = false;
    std::uint8_t best_channel = 0;
    std::int32_t best_score = std::numeric_limits<std::int32_t>::min();
    for (std::uint8_t c = 0; c < afh::kChannelCount; ++c) {
      const std::int32_t score = reprobeScore(c, now_ms);
      if (score == std::numeric_limits<std::int32_t>::min()) continue;
      if (!found || score > best_score || (score == best_score && c < best_channel)) {
        found = true;
        best_channel = c;
        best_score = score;
      }
    }
    if (found) *out_channel = best_channel;
    return found;
  }

  bool beginProbe(std::uint8_t c, std::uint32_t now_ms) {
    if (c >= afh::kChannelCount || !probeDue(c, now_ms)) return false;
    auto& s = channels_[c];
    s.state = ChannelState::Probe;
    s.last_probe_ms = now_ms;
    return true;
  }

  // Returns false for a stale/invalid probe result so callers can count it.
  bool observeProbe(std::uint8_t c, bool success, std::uint32_t now_ms) {
    if (c >= afh::kChannelCount) return false;
    auto& s = channels_[c];
    if (s.state != ChannelState::Probe) return false;

    s.last_probe_ms = now_ms;
    s.last_seen_ms = now_ms;
    ewma(s.pdr_fast_q15, success, config_.alpha_fast_shift);
    ewma(s.pdr_slow_q15, success, config_.alpha_slow_shift);
    s.probe_history_bits = static_cast<std::uint8_t>(
        ((s.probe_history_bits << 1U) | (success ? 1U : 0U)) & 0x07U);
    if (s.probe_history_count < 3U) ++s.probe_history_count;

    if (success) {
      s.probe_failure_exp = 0;
    } else if (s.probe_failure_exp < 7U) {
      ++s.probe_failure_exp;
    }

    const bool reinstate = s.probe_history_count >= 3U &&
                           popcount3(s.probe_history_bits) >= 2U;
    if (reinstate) {
      s.state = ChannelState::Active;
      if (active_count_ < afh::kChannelCount) ++active_count_;
      s.consecutive_losses = 0;
      s.consecutive_successes = 0;
      s.probe_history_bits = 0;
      s.probe_history_count = 0;
      return true;
    }

    s.state = ChannelState::Excluded;
    return true;
  }

  afh::ChannelMap activeMap() const {
    afh::ChannelMap map{0};
    for (std::uint8_t c = 0; c < afh::kChannelCount; ++c) {
      if (channels_[c].state == ChannelState::Active ||
          channels_[c].state == ChannelState::Suspect) {
        map.bits |= (1ULL << c);
      }
    }
    return map;
  }

 private:
  Config config_{};
  std::array<ChannelStats, afh::kChannelCount> channels_{};
  std::uint8_t active_count_ = afh::kChannelCount;

  static std::uint32_t elapsedMs(std::uint32_t now_ms, std::uint32_t then_ms) {
    return now_ms - then_ms;
  }

  void excludeChannel(std::uint8_t c, std::uint32_t now_ms) {
    auto& s = channels_[c];
    if (s.state == ChannelState::Excluded || s.state == ChannelState::Probe) return;
    s.state = ChannelState::Excluded;
    if (active_count_ > 0U) --active_count_;
    s.last_probe_ms = now_ms;
    s.probe_failure_exp = 0;
    s.probe_history_bits = 0;
    s.probe_history_count = 0;
    s.consecutive_successes = 0;
  }

  static void ewma(std::uint16_t& current, bool success, std::uint8_t shift) {
    const std::uint8_t safe_shift = shift > 15U ? 15U : shift;
    const std::int32_t target = success ? 32767 : 0;
    const std::int32_t value = static_cast<std::int32_t>(current);
    const std::int32_t divisor = static_cast<std::int32_t>(1U << safe_shift);
    std::int32_t next = value + (target - value) / divisor;
    if (next < 0) next = 0;
    if (next > 32767) next = 32767;
    current = static_cast<std::uint16_t>(next);
  }

  std::uint32_t probeIntervalMs(const ChannelStats& s) const {
    std::uint64_t interval = config_.initial_probe_ms;
    const std::uint8_t exp = s.probe_failure_exp > 31U ? 31U : s.probe_failure_exp;
    interval <<= exp;
    if (interval > config_.max_probe_ms) interval = config_.max_probe_ms;
    return static_cast<std::uint32_t>(interval);
  }

  std::uint16_t neighborQualityQ15(std::uint8_t c) const {
    std::uint32_t sum = 0;
    std::uint8_t count = 0;
    if (c > 0U) {
      sum += effectiveNeighborPdr(channels_[c - 1U]);
      ++count;
    }
    if (c + 1U < afh::kChannelCount) {
      sum += effectiveNeighborPdr(channels_[c + 1U]);
      ++count;
    }
    return count == 0U ? 32767U : static_cast<std::uint16_t>(sum / count);
  }

  static std::uint16_t effectiveNeighborPdr(const ChannelStats& s) {
    if (s.state == ChannelState::Excluded || s.state == ChannelState::Probe) return 0;
    return s.pdr_slow_q15;
  }

  static std::uint8_t popcount3(std::uint8_t v) {
    v &= 0x07U;
    return static_cast<std::uint8_t>((v & 1U) + ((v >> 1U) & 1U) +
                                     ((v >> 2U) & 1U));
  }
};

}  // namespace pr1::quality
