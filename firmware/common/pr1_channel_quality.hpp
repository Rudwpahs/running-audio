#pragma once

#include <array>
#include <cstdint>

#include "pr1_afh.hpp"

namespace pr1::quality {

enum class ChannelState : std::uint8_t { Active, Suspect, Excluded, Probe };

struct Config {
  std::uint8_t alpha_fast_shift = 2;
  std::uint8_t alpha_slow_shift = 5;
  std::uint16_t suspect_pdr_q15 = 30000;
  std::uint16_t exclude_pdr_q15 = 27853;
  std::uint8_t suspect_losses = 2;
  std::uint8_t exclude_losses = 4;
  std::uint32_t initial_probe_ms = 200;
  std::uint32_t max_probe_ms = 3200;
  std::uint8_t minimum_active_channels = 12;
};

struct ChannelStats {
  std::uint16_t pdr_fast_q15 = 32767;
  std::uint16_t pdr_slow_q15 = 32767;
  std::uint8_t consecutive_losses = 0;
  std::uint32_t last_seen_ms = 0;
  std::uint32_t last_probe_ms = 0;
  std::uint8_t probe_failure_exp = 0;
  ChannelState state = ChannelState::Active;
  std::uint8_t probe_history_bits = 0;
  std::uint8_t probe_history_count = 0;
};

class Estimator {
 public:
  explicit Estimator(Config config = {}) : config_(config) {}

  const ChannelStats& channel(std::uint8_t c) const { return channels_[c]; }
  std::uint8_t activeCount() const { return active_count_; }

  void observeData(std::uint8_t c, bool success, std::uint32_t now_ms) {
    if (c >= afh::kChannelCount) return;
    auto& s = channels_[c];
    s.last_seen_ms = now_ms;
    ewma(s.pdr_fast_q15, success, config_.alpha_fast_shift);
    ewma(s.pdr_slow_q15, success, config_.alpha_slow_shift);
    if (success) s.consecutive_losses = 0;
    else if (s.consecutive_losses < 255U) ++s.consecutive_losses;

    if (s.state == ChannelState::Active &&
        (s.consecutive_losses >= config_.suspect_losses ||
         s.pdr_fast_q15 < config_.suspect_pdr_q15)) {
      s.state = ChannelState::Suspect;
    } else if (s.state == ChannelState::Suspect) {
      if (success && s.pdr_fast_q15 >= config_.suspect_pdr_q15) {
        s.state = ChannelState::Active;
      } else if ((s.consecutive_losses >= config_.exclude_losses ||
                  s.pdr_fast_q15 < config_.exclude_pdr_q15) &&
                 active_count_ > config_.minimum_active_channels) {
        s.state = ChannelState::Excluded;
        --active_count_;
        s.last_probe_ms = now_ms;
      }
    }
  }

  bool probeDue(std::uint8_t c, std::uint32_t now_ms) const {
    if (c >= afh::kChannelCount) return false;
    const auto& s = channels_[c];
    if (s.state != ChannelState::Excluded && s.state != ChannelState::Probe) return false;
    const std::uint32_t interval = probeIntervalMs(s);
    return static_cast<std::int32_t>(now_ms - s.last_probe_ms) >=
           static_cast<std::int32_t>(interval);
  }

  void beginProbe(std::uint8_t c, std::uint32_t now_ms) {
    if (c >= afh::kChannelCount) return;
    auto& s = channels_[c];
    if (s.state == ChannelState::Excluded || s.state == ChannelState::Probe) {
      s.state = ChannelState::Probe;
      s.last_probe_ms = now_ms;
    }
  }

  void observeProbe(std::uint8_t c, bool success, std::uint32_t now_ms) {
    if (c >= afh::kChannelCount) return;
    auto& s = channels_[c];
    s.last_probe_ms = now_ms;
    s.probe_history_bits = static_cast<std::uint8_t>(((s.probe_history_bits << 1U) |
                                (success ? 1U : 0U)) & 0x07U);
    if (s.probe_history_count < 3U) ++s.probe_history_count;
    if (success) {
      s.probe_failure_exp = 0;
    } else if (s.probe_failure_exp < 7U) {
      ++s.probe_failure_exp;
    }
    if (s.probe_history_count >= 3U && popcount3(s.probe_history_bits) >= 2U) {
      s.state = ChannelState::Active;
      ++active_count_;
      s.consecutive_losses = 0;
      s.pdr_fast_q15 = config_.suspect_pdr_q15;
    } else {
      s.state = ChannelState::Excluded;
    }
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

  static void ewma(std::uint16_t& current, bool success, std::uint8_t shift) {
    const std::int32_t target = success ? 32767 : 0;
    const std::int32_t value = static_cast<std::int32_t>(current);
    current = static_cast<std::uint16_t>(value + ((target - value) >> shift));
  }
  std::uint32_t probeIntervalMs(const ChannelStats& s) const {
    std::uint64_t interval = static_cast<std::uint64_t>(config_.initial_probe_ms) << s.probe_failure_exp;
    if (interval > config_.max_probe_ms) interval = config_.max_probe_ms;
    return static_cast<std::uint32_t>(interval);
  }
  static std::uint8_t popcount3(std::uint8_t v) {
    v &= 0x07U;
    return static_cast<std::uint8_t>((v & 1U) + ((v >> 1U) & 1U) + ((v >> 2U) & 1U));
  }
};

}  // namespace pr1::quality
