#include <array>
#include <cassert>
#include <cstdint>
#include <iostream>

#include "../firmware/common/pr1_channel_quality.hpp"

using pr1::quality::ChannelState;

static pr1::quality::Config aggressiveConfig() {
  pr1::quality::Config cfg{};
  cfg.suspect_losses = 1;
  cfg.exclude_losses = 2;
  cfg.recover_successes = 2;
  cfg.recover_pdr_q15 = 20000;
  return cfg;
}

static void exclude(pr1::quality::Estimator& q, std::uint8_t c,
                    std::uint32_t at_ms) {
  q.observeData(c, false, at_ms);
  q.observeData(c, false, at_ms + 10U);
  assert(q.channel(c).state == ChannelState::Excluded);
}

int main() {
  {
    auto cfg = aggressiveConfig();
    pr1::quality::Estimator q(cfg);
    q.observeData(3, false, 0);
    assert(q.channel(3).state == ChannelState::Suspect);
    q.observeData(3, false, 10);
    assert(q.channel(3).state == ChannelState::Excluded);
    assert(q.activeCount() == 39);
    assert(!q.probeDue(3, 100));
    assert(q.probeDue(3, 210));

    assert(q.beginProbe(3, 210));
    assert(q.observeProbe(3, true, 210));
    assert(q.channel(3).state == ChannelState::Excluded);
    assert(q.beginProbe(3, 410));
    assert(q.observeProbe(3, true, 410));
    assert(q.channel(3).state == ChannelState::Excluded);
    assert(q.beginProbe(3, 610));
    assert(q.observeProbe(3, false, 610));
    assert(q.channel(3).state == ChannelState::Active);  // 2 of last 3 probes.
    assert(q.activeCount() == 40);
    assert(q.activeMap().activeCount() == 40);
  }

  // Probe results are only accepted after beginProbe(); this prevents a stale
  // result from double-incrementing active_count_ after reinclusion.
  {
    auto cfg = aggressiveConfig();
    pr1::quality::Estimator q(cfg);
    exclude(q, 5, 0);
    assert(!q.observeProbe(5, true, 500));
    assert(q.activeCount() == 39);
  }

  // Failed probes back off 200 -> 400 -> 800ms, but never exceed 3.2s.
  {
    auto cfg = aggressiveConfig();
    pr1::quality::Estimator q(cfg);
    exclude(q, 6, 0);
    assert(q.beginProbe(6, 210));
    assert(q.observeProbe(6, false, 210));
    assert(!q.probeDue(6, 609));
    assert(q.probeDue(6, 610));
    assert(q.beginProbe(6, 610));
    assert(q.observeProbe(6, false, 610));
    assert(!q.probeDue(6, 1409));
    assert(q.probeDue(6, 1410));
  }

  // A newly excluded channel starts a fresh 3-probe history instead of
  // inheriting probe successes from a previous exclusion cycle.
  {
    auto cfg = aggressiveConfig();
    pr1::quality::Estimator q(cfg);
    exclude(q, 7, 0);
    assert(q.beginProbe(7, 210)); assert(q.observeProbe(7, true, 210));
    assert(q.beginProbe(7, 410)); assert(q.observeProbe(7, true, 410));
    assert(q.beginProbe(7, 610)); assert(q.observeProbe(7, true, 610));
    assert(q.channel(7).state == ChannelState::Active);

    // Re-exclude. Old 3/3 probe history must be gone.
    q.observeData(7, false, 700);
    q.observeData(7, false, 710);
    assert(q.channel(7).state == ChannelState::Excluded);
    assert(q.beginProbe(7, 910));
    assert(q.observeProbe(7, false, 910));
    assert(q.channel(7).state == ChannelState::Excluded);
  }

  // Minimum active-channel floor must be enforced.
  {
    auto cfg = aggressiveConfig();
    cfg.minimum_active_channels = 38;
    pr1::quality::Estimator q(cfg);
    exclude(q, 0, 0);
    exclude(q, 1, 20);
    assert(q.activeCount() == 38);
    q.observeData(2, false, 40);
    q.observeData(2, false, 50);
    assert(q.channel(2).state == ChannelState::Suspect);
    assert(q.activeCount() == 38);
  }

  // Among due channels, informed selection favors a channel with healthier
  // neighbors/history when probe age is otherwise equal.
  {
    auto cfg = aggressiveConfig();
    pr1::quality::Estimator q(cfg);
    exclude(q, 10, 0);
    exclude(q, 20, 0);
    // Damage neighbors around channel 10 so its neighborhood score falls.
    exclude(q, 9, 20);
    exclude(q, 11, 40);
    std::uint8_t selected = 255;
    assert(q.nextProbeChannel(250, &selected));
    assert(selected == 20);
  }

  // Micro-probe wire format is fixed-size, independent of audio/FEC packets.
  {
    pr1::quality::MicroProbe probe{12, 0x1234, 0xBEEF};
    std::array<std::uint8_t, pr1::quality::kMicroProbeBytes> bytes{};
    assert(pr1::quality::encodeMicroProbe(probe, &bytes));
    pr1::quality::MicroProbe decoded{};
    assert(pr1::quality::decodeMicroProbe(bytes.data(), bytes.size(), &decoded));
    assert(decoded.channel == 12);
    assert(decoded.map_version == 0x1234);
    assert(decoded.token == 0xBEEF);
    bytes[0] ^= 0x01U;
    assert(!pr1::quality::decodeMicroProbe(bytes.data(), bytes.size(), &decoded));
  }

  // uint32 time wrap must not break probe scheduling.
  {
    auto cfg = aggressiveConfig();
    pr1::quality::Estimator q(cfg);
    const std::uint32_t near_wrap = 0xFFFFFF00U;
    q.observeData(30, false, near_wrap);
    q.observeData(30, false, near_wrap + 10U);
    assert(q.channel(30).state == ChannelState::Excluded);
    const std::uint32_t after_wrap = 0x00000020U;
    assert(q.probeDue(30, after_wrap));
  }

  std::cout << "test_channel_quality: PASS\n";
}
