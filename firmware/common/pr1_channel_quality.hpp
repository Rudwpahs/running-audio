#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>

#include "pr1_afh.hpp"
#include "pr1_placement.hpp"

namespace pr1::quality {

enum class ChannelState : std::uint8_t { Active, Suspect, Excluded, Probe };

// Which rule excluded a channel most recently (diagnostics / offline replay only).
enum class ExclusionReason : std::uint8_t {
  None, ConsecutiveLosses, FastPdr, SlowPdr, Neighbor, Probation
};

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
  // Gate C2 knobs (defaults = Gate C behaviour). A SUSPECT channel is also excluded when its
  // slow EWMA is below this: persistent moderate loss over ~2^alpha_slow_shift visits
  // (0 = no slow-EWMA rule).
  std::uint16_t exclude_slow_pdr_q15 = 0;
  // Probe successes, out of the last 3 probes, needed to re-include a channel.
  std::uint8_t reinstate_probe_successes = 2;

  // Gate C3 knobs (issue #52). Every default reproduces Gate C/C2 behaviour exactly
  // (tests/test_channel_quality_c3.cpp::defaultsUnchanged).
  //
  // (1) Exclusion memory. Every exclusion adds a strike to the channel; strikes survive
  // re-inclusion. With k >= 2 strikes the probe interval base becomes
  // max(initial_probe_ms, strike_probe_ms), it is multiplied by
  // 2^min(k - 1, strike_backoff_max_shift) and capped at strike_max_probe_ms instead of
  // max_probe_ms (0 = keep max_probe_ms; a non-zero value replaces the cap even when it is
  // lower than max_probe_ms). strike_backoff_max_shift = 0 = no strike backoff.
  std::uint8_t strike_backoff_max_shift = 0;
  std::uint32_t strike_probe_ms = 0;
  std::uint32_t strike_max_probe_ms = 0;
  // Forgiveness: one strike is removed after every strike_decay_ms of continuous *clean*
  // inclusion: the clock runs only while the channel is Active and its data frames arrive.
  // Any data loss, and every visit in Suspect, restarts it, so a channel that is held in
  // Suspect because the map is at the minimum-active floor earns no forgiveness.
  // 0 = strikes never decay.
  std::uint32_t strike_decay_ms = 0;
  //
  // (2) Neighbour corroboration. Evaluated only on a data frame that was LOST on the channel
  // itself while it is in the map. The channel is excluded when
  //   - it has direct evidence of its own: the current frame is a loss, it has at least N
  //     data losses since its (re)inclusion, and its fast EWMA is below
  //     neighbor_direct_fast_q15. N is the number of losses that take a clean fast EWMA
  //     below that threshold (1 for thresholds above 0.75 with alpha 1/4), so a stale EWMA
  //     left over from before a quarantine can never stand in for a new loss; and
  //   - at least neighbor_min_bad (0 is treated as 1) channels within +/-neighbor_radius
  //     (clamped to 2) are bad: excluded/probing, or slow EWMA below neighbor_bad_slow_q15.
  // Neighbour evidence alone never excludes, and never on a received frame.
  // neighbor_radius = 0 or neighbor_direct_fast_q15 = 0 = off; thresholds above 1.0 are
  // clamped to 1.0 (= "one own loss").
  std::uint8_t neighbor_radius = 0;
  std::uint8_t neighbor_min_bad = 1;
  std::uint16_t neighbor_bad_slow_q15 = 0;
  std::uint16_t neighbor_direct_fast_q15 = 0;
  //
  // (3) Re-entry. Probe history length (1..8) over which reinstate_probe_successes are
  // counted (3 = Gate C/C2). After re-inclusion a channel is on probation for
  // probation_visits data visits: one loss in that period re-excludes it (with a strike).
  // 0 = no probation.
  std::uint8_t reinstate_probe_window = 3;
  std::uint8_t probation_visits = 0;
  // Non-zero: a re-included channel starts with clean fast/slow EWMAs, so the fast/slow PDR
  // rules cannot re-exclude it on pre-quarantine losses (with the C2 values the stale slow
  // EWMA otherwise re-excludes it on its second received frame). Its history is carried by
  // the strikes instead. 0 = keep the EWMAs (Gate C/C2).
  std::uint8_t reinstate_reset_pdr = 0;

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
  // Gate C3 state (inert with the default Config).
  std::uint8_t strikes = 0;
  std::uint8_t probation_left = 0;
  ExclusionReason last_exclusion_reason = ExclusionReason::None;
  // Start of the current clean-inclusion stretch (strike forgiveness clock).
  std::uint32_t included_since_ms = 0;
  // Data losses seen while in the map since the last (re)inclusion (saturating).
  std::uint8_t losses_since_inclusion = 0;
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
  explicit Estimator(Config config = {}) : config_(config) { deriveConfig(); }

  const Config& config() const { return config_; }
  void setConfig(const Config& config) {
    config_ = config;
    deriveConfig();
  }

  const ChannelStats& channel(std::uint8_t c) const {
    if (c < afh::kChannelCount) return channels_[c];
    static const ChannelStats invalid{};
    return invalid;
  }
  std::uint8_t activeCount() const { return active_count_; }

  PR1_IRAM void observeData(std::uint8_t c, bool success, std::uint32_t now_ms) {
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

    if (s.state == ChannelState::Active || s.state == ChannelState::Suspect) {
      if (!success && s.losses_since_inclusion < 255U) ++s.losses_since_inclusion;
      if (config_.strike_decay_ms != 0U) {
        if (!success || s.state == ChannelState::Suspect) {
          s.included_since_ms = now_ms;  // forgiveness needs clean Active time
        } else if (s.strikes > 0U &&
                   elapsedMs(now_ms, s.included_since_ms) >= config_.strike_decay_ms) {
          --s.strikes;
          s.included_since_ms = now_ms;
        }
      }
      if (s.probation_left > 0U) {
        if (!success && active_count_ > config_.minimum_active_channels) {
          excludeChannel(c, now_ms, ExclusionReason::Probation);
          return;
        }
        if (success) --s.probation_left;
      }
    }

    if (s.state == ChannelState::Active) {
      if (s.consecutive_losses >= config_.suspect_losses ||
          s.pdr_fast_q15 < config_.suspect_pdr_q15) {
        s.state = ChannelState::Suspect;
        s.consecutive_successes = 0;
        if (neighborCorroborated(c, success) &&
            active_count_ > config_.minimum_active_channels) {
          excludeChannel(c, now_ms, ExclusionReason::Neighbor);
        }
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

    ExclusionReason reason = ExclusionReason::None;
    if (s.consecutive_losses >= config_.exclude_losses) {
      reason = ExclusionReason::ConsecutiveLosses;
    } else if (s.pdr_fast_q15 < config_.exclude_pdr_q15) {
      reason = ExclusionReason::FastPdr;
    } else if (s.pdr_slow_q15 < config_.exclude_slow_pdr_q15) {
      reason = ExclusionReason::SlowPdr;
    } else if (neighborCorroborated(c, success)) {
      reason = ExclusionReason::Neighbor;
    }
    if (reason != ExclusionReason::None && active_count_ > config_.minimum_active_channels) {
      excludeChannel(c, now_ms, reason);
    }
  }

  PR1_IRAM bool probeDue(std::uint8_t c, std::uint32_t now_ms) const {
    if (c >= afh::kChannelCount) return false;
    const auto& s = channels_[c];
    if (s.state != ChannelState::Excluded && s.state != ChannelState::Probe) {
      return false;
    }
    return elapsedMs(now_ms, s.last_probe_ms) >= probeIntervalMs(s);
  }

  // Returns a relative priority only. It is intentionally simple/integer-only
  // so it can run cheaply on ESP32-S3 and be tuned from hardware data later.
  PR1_IRAM std::int32_t reprobeScore(std::uint8_t c, std::uint32_t now_ms) const {
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

  PR1_IRAM bool nextProbeChannel(std::uint32_t now_ms, std::uint8_t* out_channel) const {
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

  PR1_IRAM bool beginProbe(std::uint8_t c, std::uint32_t now_ms) {
    if (c >= afh::kChannelCount || !probeDue(c, now_ms)) return false;
    auto& s = channels_[c];
    s.state = ChannelState::Probe;
    s.last_probe_ms = now_ms;
    return true;
  }

  // Returns false for a stale/invalid probe result so callers can count it.
  PR1_IRAM bool observeProbe(std::uint8_t c, bool success, std::uint32_t now_ms) {
    if (c >= afh::kChannelCount) return false;
    auto& s = channels_[c];
    if (s.state != ChannelState::Probe) return false;

    s.last_probe_ms = now_ms;
    s.last_seen_ms = now_ms;
    ewma(s.pdr_fast_q15, success, config_.alpha_fast_shift);
    ewma(s.pdr_slow_q15, success, config_.alpha_slow_shift);
    const std::uint8_t window = probeWindow();
    const std::uint32_t mask = (1U << window) - 1U;
    s.probe_history_bits = static_cast<std::uint8_t>(
        ((static_cast<std::uint32_t>(s.probe_history_bits) << 1U) | (success ? 1U : 0U)) & mask);
    if (s.probe_history_count < window) ++s.probe_history_count;

    if (success) {
      s.probe_failure_exp = 0;
    } else if (s.probe_failure_exp < 7U) {
      ++s.probe_failure_exp;
    }

    const bool reinstate = s.probe_history_count >= window &&
                           popcount8(s.probe_history_bits) >= config_.reinstate_probe_successes;
    if (reinstate) {
      s.state = ChannelState::Active;
      if (active_count_ < afh::kChannelCount) ++active_count_;
      s.consecutive_losses = 0;
      s.consecutive_successes = 0;
      s.probe_history_bits = 0;
      s.probe_history_count = 0;
      s.probation_left = config_.probation_visits;
      s.included_since_ms = now_ms;
      s.losses_since_inclusion = 0;
      if (config_.reinstate_reset_pdr != 0U) {
        s.pdr_fast_q15 = 32767U;
        s.pdr_slow_q15 = 32767U;
      }
      return true;
    }

    s.state = ChannelState::Excluded;
    return true;
  }

  PR1_IRAM afh::ChannelMap activeMap() const {
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
  // Derived from config_: own data losses since inclusion needed as direct evidence for the
  // neighbour rule (0xFF = rule off / unreachable threshold).
  std::uint8_t neighbor_direct_min_losses_ = 0xFF;
  std::uint16_t neighbor_direct_fast_q15_ = 0;

  void deriveConfig() {
    neighbor_direct_min_losses_ = 0xFF;
    neighbor_direct_fast_q15_ =
        config_.neighbor_direct_fast_q15 > 32767U ? 32767U : config_.neighbor_direct_fast_q15;
    if (config_.neighbor_radius == 0U || neighbor_direct_fast_q15_ == 0U) return;
    std::uint16_t v = 32767U;
    for (std::uint8_t n = 1; n <= 64U; ++n) {
      ewma(v, false, config_.alpha_fast_shift);
      if (v < neighbor_direct_fast_q15_) {
        neighbor_direct_min_losses_ = n;
        return;
      }
    }
  }

  PR1_IRAM static std::uint32_t elapsedMs(std::uint32_t now_ms, std::uint32_t then_ms) {
    return now_ms - then_ms;
  }

  PR1_IRAM void excludeChannel(std::uint8_t c, std::uint32_t now_ms, ExclusionReason reason) {
    auto& s = channels_[c];
    if (s.state == ChannelState::Excluded || s.state == ChannelState::Probe) return;
    s.state = ChannelState::Excluded;
    s.last_exclusion_reason = reason;
    if (s.strikes < 255U) ++s.strikes;
    s.probation_left = 0;
    if (active_count_ > 0U) --active_count_;
    s.last_probe_ms = now_ms;
    s.probe_failure_exp = 0;
    s.probe_history_bits = 0;
    s.probe_history_count = 0;
    s.consecutive_successes = 0;
  }

  PR1_IRAM static void ewma(std::uint16_t& current, bool success, std::uint8_t shift) {
    const std::uint8_t safe_shift = shift > 15U ? 15U : shift;
    const std::int32_t target = success ? 32767 : 0;
    const std::int32_t value = static_cast<std::int32_t>(current);
    const std::int32_t divisor = static_cast<std::int32_t>(1U << safe_shift);
    std::int32_t next = value + (target - value) / divisor;
    if (next < 0) next = 0;
    if (next > 32767) next = 32767;
    current = static_cast<std::uint16_t>(next);
  }

  PR1_IRAM std::uint32_t probeIntervalMs(const ChannelStats& s) const {
    std::uint64_t interval = config_.initial_probe_ms;
    std::uint32_t exp = s.probe_failure_exp;
    std::uint64_t cap = config_.max_probe_ms;
    if (config_.strike_backoff_max_shift != 0U && s.strikes > 1U) {
      if (config_.strike_probe_ms > interval) interval = config_.strike_probe_ms;
      const std::uint32_t extra = static_cast<std::uint32_t>(s.strikes) - 1U;
      exp += extra < config_.strike_backoff_max_shift ? extra : config_.strike_backoff_max_shift;
      if (config_.strike_max_probe_ms != 0U) cap = config_.strike_max_probe_ms;
    }
    if (exp > 31U) exp = 31U;
    interval <<= exp;
    if (interval > cap) interval = cap;
    return static_cast<std::uint32_t>(interval);
  }

  PR1_IRAM std::uint8_t probeWindow() const {
    const std::uint8_t w = config_.reinstate_probe_window;
    return w == 0U ? 1U : (w > 8U ? 8U : w);
  }

  // Band corroboration: direct evidence on c (this frame lost, enough losses since its
  // (re)inclusion) plus >= neighbor_min_bad bad neighbours. Only called for a channel that is
  // in the map (Active/Suspect), after losses_since_inclusion was updated for this frame.
  PR1_IRAM bool neighborCorroborated(std::uint8_t c, bool success) const {
    if (success || neighbor_direct_min_losses_ == 0xFFU) return false;
    const auto& s = channels_[c];
    if (s.losses_since_inclusion < neighbor_direct_min_losses_) return false;
    if (s.pdr_fast_q15 >= neighbor_direct_fast_q15_) return false;
    const int r = config_.neighbor_radius > 2U ? 2 : static_cast<int>(config_.neighbor_radius);
    const std::uint8_t min_bad = config_.neighbor_min_bad == 0U ? 1U : config_.neighbor_min_bad;
    std::uint8_t bad = 0;
    for (int j = static_cast<int>(c) - r; j <= static_cast<int>(c) + r; ++j) {
      if (j < 0 || j >= static_cast<int>(afh::kChannelCount) || j == static_cast<int>(c)) continue;
      const auto& n = channels_[static_cast<std::size_t>(j)];
      if (n.state == ChannelState::Excluded || n.state == ChannelState::Probe ||
          n.pdr_slow_q15 < config_.neighbor_bad_slow_q15) {
        ++bad;
      }
    }
    return bad >= min_bad;
  }

  PR1_IRAM std::uint16_t neighborQualityQ15(std::uint8_t c) const {
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

  PR1_IRAM static std::uint16_t effectiveNeighborPdr(const ChannelStats& s) {
    if (s.state == ChannelState::Excluded || s.state == ChannelState::Probe) return 0;
    return s.pdr_slow_q15;
  }

  PR1_IRAM static std::uint8_t popcount8(std::uint8_t v) {
    std::uint8_t n = 0;
    for (; v != 0U; v = static_cast<std::uint8_t>(v & (v - 1U))) ++n;
    return n;
  }
};

}  // namespace pr1::quality
