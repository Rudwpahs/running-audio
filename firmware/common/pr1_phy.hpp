#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace pr1::phy {

enum class ProfileId : std::uint8_t { Flrc1300Cr34, Flrc650Cr34, Flrc520Cr34, Flrc325Cr34 };

struct Profile {
  ProfileId id;
  std::uint32_t raw_bitrate_bps;
  std::uint8_t code_num;
  std::uint8_t code_den;
  bool emergency_only;
};

constexpr std::array<Profile, 4> kProfiles{{
    {ProfileId::Flrc1300Cr34, 1300000U, 3, 4, false},
    {ProfileId::Flrc650Cr34, 650000U, 3, 4, false},
    {ProfileId::Flrc520Cr34, 520000U, 3, 4, false},
    {ProfileId::Flrc325Cr34, 325000U, 3, 4, true},
}};

inline const Profile& profile(ProfileId id) {
  return kProfiles[static_cast<std::size_t>(id)];
}

// Host-side planning estimate only. Real firmware must replace/validate this
// with measured RadioLib/SX1280 timings including configured preamble/sync/CRC.
inline std::uint32_t estimateAirtimeUs(ProfileId id, std::size_t payload_bytes,
                                       std::size_t framing_bytes = 8U) {
  const auto& p = profile(id);
  const std::uint64_t coded_bits = static_cast<std::uint64_t>(payload_bytes + framing_bytes) * 8ULL * p.code_den;
  const std::uint64_t useful_rate_num = static_cast<std::uint64_t>(p.raw_bitrate_bps) * p.code_num;
  return static_cast<std::uint32_t>((coded_bits * 1000000ULL + useful_rate_num - 1ULL) / useful_rate_num);
}

enum class LinkClass : std::uint8_t { Healthy, Interference, WeakLink, Burst };

struct LinkSnapshot {
  std::uint16_t per_permille = 0;
  std::uint16_t bad_channel_permille = 0;
  std::int16_t rssi_margin_db_x10 = 200;
  std::uint8_t consecutive_loss_max = 0;
};

inline LinkClass classify(const LinkSnapshot& s) {
  if (s.consecutive_loss_max >= 2U) return LinkClass::Burst;
  if (s.bad_channel_permille >= 200U && s.rssi_margin_db_x10 >= 100) return LinkClass::Interference;
  if ((s.per_permille >= 30U && s.rssi_margin_db_x10 < 100) ||
      s.bad_channel_permille >= 600U) return LinkClass::WeakLink;
  return LinkClass::Healthy;
}

class Ladder {
 public:
  explicit Ladder(std::uint32_t cooldown_ms = 2000U) : cooldown_ms_(cooldown_ms) {}
  ProfileId current() const { return current_; }

  bool update(LinkClass link, std::uint32_t now_ms) {
    if (static_cast<std::int32_t>(now_ms - last_change_ms_) < static_cast<std::int32_t>(cooldown_ms_)) return false;
    ProfileId next = current_;
    if (link == LinkClass::WeakLink || link == LinkClass::Burst) {
      if (current_ == ProfileId::Flrc1300Cr34) next = ProfileId::Flrc650Cr34;
      else if (current_ == ProfileId::Flrc650Cr34) next = ProfileId::Flrc520Cr34;
    } else if (link == LinkClass::Healthy) {
      if (current_ == ProfileId::Flrc520Cr34) next = ProfileId::Flrc650Cr34;
      else if (current_ == ProfileId::Flrc650Cr34) next = ProfileId::Flrc1300Cr34;
    }
    if (next == current_) return false;
    current_ = next;
    last_change_ms_ = now_ms;
    return true;
  }

 private:
  ProfileId current_ = ProfileId::Flrc1300Cr34;
  std::uint32_t cooldown_ms_ = 2000U;
  std::uint32_t last_change_ms_ = 0U - 2000U;
};

}  // namespace pr1::phy
