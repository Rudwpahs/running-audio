#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "pr1_phy.hpp"

namespace pr1::controller {

enum class State : std::uint8_t { Good, Interference, WeakLink, Burst, Recovery };

struct FeatureFlags {
  bool adaptive_map = true;
  bool xor_fec = true;
  bool deadline_arq = true;
  bool adaptive_phy = true;
  bool adaptive_jitter = false;
  bool probing = true;
};

struct Config {
  std::uint16_t degraded_per_permille = 30;
  std::uint16_t severe_per_permille = 100;
  std::uint16_t good_per_permille = 10;
  std::uint16_t interference_bad_channel_permille = 200;
  std::uint16_t weak_bad_channel_permille = 600;
  std::int16_t healthy_margin_db_x10 = 100;
  std::uint8_t burst_trigger = 2;
  std::uint32_t min_state_dwell_ms = 1000;
  std::uint32_t good_recovery_hold_ms = 2000;
  std::uint8_t max_airtime_percent = 65;
};

struct Metrics {
  std::uint16_t per_200ms_permille = 0;
  std::uint16_t per_1s_permille = 0;
  std::uint16_t post_fec_per_permille = 0;
  std::uint16_t post_arq_per_permille = 0;
  std::uint8_t burst_max = 0;
  std::int16_t rssi_margin_db_x10 = 200;
  std::uint16_t bad_channel_permille = 0;
  std::uint8_t active_channel_count = 40;
  std::uint16_t jitter_us = 0;
  std::uint8_t buffer_frames = 4;
  std::uint8_t airtime_percent = 0;
  std::uint8_t radio_queue_depth = 0;
  std::uint32_t irq_to_spi_p99_us = 0;
};

struct Actions {
  phy::ProfileId phy_profile = phy::ProfileId::Flrc1300Cr34;
  bool adaptive_map = true;
  bool aggressive_probe = false;
  bool xor_fec = false;
  bool deadline_arq = true;
  bool interleave_depth2 = false;
  std::uint16_t jitter_target_ms = 40;
};

struct Transition {
  State from = State::Good;
  State to = State::Good;
  std::uint32_t at_ms = 0;
  std::uint16_t per_permille = 0;
  std::uint16_t bad_channel_permille = 0;
  std::int16_t rssi_margin_db_x10 = 0;
  std::uint8_t burst_max = 0;
};

class LinkController {
 public:
  LinkController(Config config = {}, FeatureFlags flags = {}) : config_(config), flags_(flags) {}

  State state() const { return state_; }
  const Actions& actions() const { return actions_; }
  std::size_t transitionCount() const { return transition_count_; }

  bool transitionAt(std::size_t i, Transition* out) const {
    if (out == nullptr || i >= transition_count_) return false;
    const std::size_t oldest = transition_count_ == transitions_.size() ? transition_write_ : 0U;
    *out = transitions_[(oldest + i) % transitions_.size()];
    return true;
  }

  void update(const Metrics& m, std::uint32_t now_ms) {
    const State desired = classify(m);
    if (desired != state_ &&
        static_cast<std::int32_t>(now_ms - last_transition_ms_) >= static_cast<std::int32_t>(config_.min_state_dwell_ms)) {
      State target = desired;
      if (state_ != State::Good && desired == State::Good) target = State::Recovery;
      if (state_ == State::Recovery && desired == State::Good &&
          static_cast<std::int32_t>(now_ms - last_transition_ms_) < static_cast<std::int32_t>(config_.good_recovery_hold_ms)) {
        target = State::Recovery;
      }
      if (target != state_) setState(target, m, now_ms);
    }
    if (state_ == State::Recovery && desired == State::Good &&
        static_cast<std::int32_t>(now_ms - last_transition_ms_) >= static_cast<std::int32_t>(config_.good_recovery_hold_ms)) {
      setState(State::Good, m, now_ms);
    }
    actions_ = actionsFor(state_, m);
  }

 private:
  Config config_{};
  FeatureFlags flags_{};
  State state_ = State::Good;
  Actions actions_{};
  std::uint32_t last_transition_ms_ = 0U - 1000U;
  std::array<Transition, 16> transitions_{};
  std::size_t transition_write_ = 0;
  std::size_t transition_count_ = 0;

  State classify(const Metrics& m) const {
    if (m.burst_max >= config_.burst_trigger || m.per_200ms_permille >= config_.severe_per_permille) return State::Burst;
    if (m.bad_channel_permille >= config_.interference_bad_channel_permille &&
        m.bad_channel_permille < config_.weak_bad_channel_permille &&
        m.rssi_margin_db_x10 >= config_.healthy_margin_db_x10) return State::Interference;
    if (m.per_1s_permille >= config_.degraded_per_permille &&
        (m.rssi_margin_db_x10 < config_.healthy_margin_db_x10 ||
         m.bad_channel_permille >= config_.weak_bad_channel_permille)) return State::WeakLink;
    return State::Good;
  }

  Actions actionsFor(State s, const Metrics& m) const {
    Actions a{};
    a.adaptive_map = flags_.adaptive_map;
    a.deadline_arq = flags_.deadline_arq;
    switch (s) {
      case State::Good:
        a.phy_profile = phy::ProfileId::Flrc1300Cr34;
        a.xor_fec = false;
        a.jitter_target_ms = 40;
        break;
      case State::Interference:
        a.phy_profile = phy::ProfileId::Flrc1300Cr34;
        a.aggressive_probe = flags_.probing;
        a.xor_fec = flags_.xor_fec && m.per_200ms_permille >= config_.degraded_per_permille;
        a.jitter_target_ms = 40;
        break;
      case State::WeakLink:
        a.phy_profile = flags_.adaptive_phy ? phy::ProfileId::Flrc650Cr34 : phy::ProfileId::Flrc1300Cr34;
        a.xor_fec = flags_.xor_fec;
        a.jitter_target_ms = 50;
        break;
      case State::Burst:
        a.phy_profile = flags_.adaptive_phy ? phy::ProfileId::Flrc520Cr34 : phy::ProfileId::Flrc1300Cr34;
        a.xor_fec = flags_.xor_fec;
        a.interleave_depth2 = false;
        a.jitter_target_ms = 60;
        break;
      case State::Recovery:
        a.phy_profile = flags_.adaptive_phy ? phy::ProfileId::Flrc650Cr34 : phy::ProfileId::Flrc1300Cr34;
        a.xor_fec = flags_.xor_fec;
        a.aggressive_probe = false;
        a.jitter_target_ms = 50;
        break;
    }

    if (m.airtime_percent >= config_.max_airtime_percent) {
      a.deadline_arq = false;
      if (s == State::Good || s == State::Interference) a.xor_fec = false;
    }
    return a;
  }

  void setState(State next, const Metrics& m, std::uint32_t now_ms) {
    const State previous = state_;
    state_ = next;
    last_transition_ms_ = now_ms;
    transitions_[transition_write_] = Transition{previous, next, now_ms, m.per_200ms_permille,
                                                 m.bad_channel_permille, m.rssi_margin_db_x10,
                                                 m.burst_max};
    transition_write_ = (transition_write_ + 1U) % transitions_.size();
    if (transition_count_ < transitions_.size()) ++transition_count_;
  }
};

}  // namespace pr1::controller
