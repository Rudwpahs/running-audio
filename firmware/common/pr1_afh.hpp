#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#ifndef PR1_ENABLE_AFH
#define PR1_ENABLE_AFH 0
#endif

namespace pr1::afh {

constexpr bool kEnabledByDefault = PR1_ENABLE_AFH != 0;
constexpr std::uint8_t kChannelCount = 40;
constexpr std::uint64_t kChannelMask = (1ULL << kChannelCount) - 1ULL;
constexpr std::uint32_t kBaseFrequencyHz = 2404000000UL;
constexpr std::uint32_t kChannelSpacingHz = 2000000UL;
constexpr std::uint8_t kExperimentalMinimumActiveChannels = 12;
constexpr std::uint8_t kRendezvousChannels[3] = {4, 20, 36};

struct ChannelMap {
  std::uint64_t bits = kChannelMask;
  bool isActive(std::uint8_t channel) const {
    return channel < kChannelCount && ((bits >> channel) & 1ULL) != 0;
  }
  std::uint8_t activeCount() const {
    std::uint64_t value = bits & kChannelMask;
    std::uint8_t count = 0;
    while (value != 0) { value &= value - 1ULL; ++count; }
    return count;
  }
  bool isValid(std::uint8_t minimum_active = kExperimentalMinimumActiveChannels) const {
    return (bits & ~kChannelMask) == 0 && activeCount() >= minimum_active;
  }
};

constexpr std::uint32_t frequencyHz(std::uint8_t channel) {
  return kBaseFrequencyHz + static_cast<std::uint32_t>(channel) * kChannelSpacingHz;
}

struct ScheduleConfig {
  std::uint64_t session_seed = 0;
  std::uint16_t session_id = 0;
  std::uint16_t map_version = 0;
  ChannelMap map{};
};

struct PendingMap {
  bool valid = false;
  std::uint16_t map_version = 0;
  std::uint32_t activation_sequence = 0;
  ChannelMap map{};
};

struct SyncBeacon {
  std::uint16_t session_id = 0;
  std::uint32_t frame_seq = 0;
  std::uint32_t epoch = 0;
  std::uint16_t map_version = 0;
};

class Scheduler {
 public:
  explicit Scheduler(const ScheduleConfig& config) : current_(config) {}
  const ScheduleConfig& current() const { return current_; }
  const PendingMap& pending() const { return pending_; }

  bool stageMap(std::uint16_t new_version, ChannelMap new_map,
                std::uint32_t activation_sequence,
                std::uint8_t minimum_active = kExperimentalMinimumActiveChannels) {
    if (!new_map.isValid(minimum_active)) return false;
    const std::uint16_t baseline = pending_.valid ? pending_.map_version : current_.map_version;
    if (!versionAfter(new_version, baseline)) return false;
    pending_ = PendingMap{true, new_version, activation_sequence, new_map};
    return true;
  }

  void applyPendingIfDue(std::uint32_t sequence) {
    if (!pending_.valid || sequenceBefore(sequence, pending_.activation_sequence)) return;
    current_.map = pending_.map;
    current_.map_version = pending_.map_version;
    pending_.valid = false;
  }

  std::uint8_t channelForSequence(std::uint32_t sequence) const {
    std::array<std::uint8_t, kChannelCount> active{};
    const std::uint8_t count = collectActive(current_.map, active);
    if (count == 0) return 0;
    const std::uint32_t epoch = sequence / count;
    const std::uint8_t position = static_cast<std::uint8_t>(sequence % count);
    buildPermutation(active, count, current_, epoch);
    if (epoch > 0U && count > 1U) {
      std::array<std::uint8_t, kChannelCount> previous{};
      collectActive(current_.map, previous);
      buildPermutation(previous, count, current_, epoch - 1U);
      if (active[0] == previous[count - 1U]) {
        const std::uint8_t tmp = active[0];
        active[0] = active[1];
        active[1] = tmp;
      }
    }
    return active[position];
  }

  std::uint8_t rendezvousChannel(std::uint32_t scan_slot) const {
    const std::uint32_t offset = static_cast<std::uint32_t>(current_.session_seed) % 3U;
    return kRendezvousChannels[(scan_slot + offset) % 3U];
  }

  bool beaconMatchesSession(const SyncBeacon& beacon) const {
    return beacon.session_id == current_.session_id;
  }

 private:
  ScheduleConfig current_{};
  PendingMap pending_{};
  static bool sequenceBefore(std::uint32_t a, std::uint32_t b) {
    return static_cast<std::int32_t>(a - b) < 0;
  }
  static bool versionAfter(std::uint16_t a, std::uint16_t b) {
    return static_cast<std::int16_t>(a - b) > 0;
  }
  static std::uint8_t collectActive(const ChannelMap& map,
                                    std::array<std::uint8_t, kChannelCount>& out) {
    std::uint8_t count = 0;
    for (std::uint8_t c = 0; c < kChannelCount; ++c) if (map.isActive(c)) out[count++] = c;
    return count;
  }
  static std::uint64_t mix64(std::uint64_t x) {
    x += 0x9E3779B97F4A7C15ULL;
    x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ULL;
    x = (x ^ (x >> 27)) * 0x94D049BB133111EBULL;
    return x ^ (x >> 31);
  }
  static std::uint64_t seedFor(const ScheduleConfig& config, std::uint32_t epoch) {
    std::uint64_t seed = mix64(config.session_seed);
    seed ^= mix64(static_cast<std::uint64_t>(config.map_version) << 32);
    seed ^= mix64(config.map.bits & kChannelMask);
    seed ^= mix64(epoch);
    return mix64(seed);
  }
  static std::uint32_t nextRandom(std::uint64_t& state) {
    state += 0x9E3779B97F4A7C15ULL;
    std::uint64_t z = state;
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    z ^= z >> 31;
    return static_cast<std::uint32_t>(z >> 32);
  }
  static void buildPermutation(std::array<std::uint8_t, kChannelCount>& channels,
                               std::uint8_t count, const ScheduleConfig& config,
                               std::uint32_t epoch) {
    if (count < 2) return;
    std::uint64_t state = seedFor(config, epoch);
    for (std::uint8_t i = static_cast<std::uint8_t>(count - 1U); i > 0; --i) {
      const std::uint8_t j = static_cast<std::uint8_t>(nextRandom(state) % (i + 1U));
      const std::uint8_t tmp = channels[i]; channels[i] = channels[j]; channels[j] = tmp;
    }
  }
};

}  // namespace pr1::afh
