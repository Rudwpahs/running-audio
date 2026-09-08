#include <cassert>
#include <iostream>

#include "../firmware/common/pr1_link_controller.hpp"

namespace {

pr1::controller::FeatureFlags allAdaptiveFeatures() {
  pr1::controller::FeatureFlags flags{};
  flags.adaptive_map = true;
  flags.xor_fec = true;
  flags.deadline_arq = true;
  flags.adaptive_phy = true;
  flags.adaptive_jitter = true;
  flags.probing = true;
  return flags;
}

}  // namespace

int main() {
  using pr1::controller::Config;
  using pr1::controller::FeatureFlags;
  using pr1::controller::LinkController;
  using pr1::controller::Metrics;
  using pr1::controller::State;

  // Pre-activation safety: having adaptive code compiled in must not enable it.
  {
    LinkController safe_default;
    Metrics clean{};
    safe_default.update(clean, 0);
    const auto& a = safe_default.actions();
    assert(safe_default.state() == State::Good);
    assert(!a.adaptive_map);
    assert(!a.aggressive_probe);
    assert(!a.xor_fec);
    assert(!a.deadline_arq);
    assert(a.phy_profile == pr1::phy::ProfileId::Flrc1300Cr34);
    assert(a.jitter_target_ms == 40);
  }

  // Explicit feature enablement preserves intentional RF-state behavior.
  {
    Config cfg{};
    LinkController c(cfg, allAdaptiveFeatures());
    Metrics m{};
    c.update(m, 0);
    assert(c.state() == State::Good);

    m.bad_channel_permille = 300;
    m.rssi_margin_db_x10 = 150;
    c.update(m, 1000);
    assert(c.state() == State::Interference);
    assert(c.actions().aggressive_probe);

    m = {};
    m.per_1s_permille = 60;
    m.rssi_margin_db_x10 = 50;
    c.update(m, 2000);
    assert(c.state() == State::WeakLink);
    assert(c.actions().xor_fec);
    assert(c.actions().phy_profile == pr1::phy::ProfileId::Flrc650Cr34);
    assert(c.actions().jitter_target_ms == 50);

    m = {};
    m.burst_max = 3;
    c.update(m, 3000);
    assert(c.state() == State::Burst);
    assert(c.actions().phy_profile == pr1::phy::ProfileId::Flrc520Cr34);
    assert(c.actions().jitter_target_ms == 60);
  }

  // Processing saturation has priority over PER/burst RF interpretations.
  {
    Config cfg{};
    cfg.processing_queue_depth_trigger = 3;
    cfg.processing_irq_to_spi_us_trigger = 200;
    cfg.processing_rx_us_trigger = 800;
    cfg.processing_rearm_us_trigger = 300;
    cfg.processing_scheduler_miss_trigger = 1;

    LinkController overloaded(cfg, allAdaptiveFeatures());
    Metrics m{};
    m.burst_max = 4;
    m.per_200ms_permille = 200;
    m.rssi_margin_db_x10 = 200;
    m.processing_metrics_valid = true;
    m.radio_queue_depth = 5;
    m.irq_to_spi_p99_us = 250;
    overloaded.update(m, 0);
    assert(overloaded.state() == State::ProcessingLimited);
    const auto& a = overloaded.actions();
    assert(a.phy_profile == pr1::phy::ProfileId::Flrc1300Cr34);
    assert(!a.aggressive_probe);
    assert(!a.xor_fec);
    assert(!a.deadline_arq);
    assert(a.jitter_target_ms == 40);

    // The same numeric values are not evidence of overload until observed/valid.
    LinkController unobserved(cfg, allAdaptiveFeatures());
    m.processing_metrics_valid = false;
    unobserved.update(m, 0);
    assert(unobserved.state() == State::Burst);
  }

  // Every processing signal can independently trip a configured overload gate.
  {
    Config cfg{};
    cfg.processing_queue_depth_trigger = 10;
    cfg.processing_irq_to_spi_us_trigger = 1000;
    cfg.processing_rx_us_trigger = 900;
    cfg.processing_rearm_us_trigger = 700;
    cfg.processing_scheduler_miss_trigger = 2;

    auto assertLimited = [&](Metrics m) {
      m.processing_metrics_valid = true;
      LinkController c(cfg, allAdaptiveFeatures());
      c.update(m, 0);
      assert(c.state() == State::ProcessingLimited);
    };

    Metrics rx{};
    rx.rx_processing_p99_us = 900;
    assertLimited(rx);
    Metrics rearm{};
    rearm.rx_rearm_p99_us = 700;
    assertLimited(rearm);
    Metrics misses{};
    misses.scheduler_misses_recent = 2;
    assertLimited(misses);
  }

  // Scheduler-miss classification consumes a recent-window delta, not the
  // lifetime instrumentation counter. A historical miss must not poison all
  // later controller updates after the recent window returns to zero.
  {
    Config cfg{};
    cfg.processing_scheduler_miss_trigger = 1;
    LinkController recent(cfg, allAdaptiveFeatures());
    Metrics spike{};
    spike.processing_metrics_valid = true;
    spike.scheduler_misses_recent = 1;
    recent.update(spike, 0);
    assert(recent.state() == State::ProcessingLimited);

    LinkController clean_window(cfg, allAdaptiveFeatures());
    Metrics clean{};
    clean.processing_metrics_valid = true;
    clean.scheduler_misses_recent = 0;
    clean_window.update(clean, 0);
    assert(clean_window.state() == State::Good);
  }

  // Recovery hysteresis must honor the configured GOOD threshold, not merely
  // the absence of another fault classifier.
  {
    Config cfg{};
    cfg.good_per_permille = 5;
    cfg.min_state_dwell_ms = 1000;
    cfg.good_recovery_hold_ms = 2000;
    LinkController c(cfg, allAdaptiveFeatures());

    Metrics weak{};
    weak.per_1s_permille = 60;
    weak.rssi_margin_db_x10 = 50;
    c.update(weak, 0);
    assert(c.state() == State::WeakLink);

    Metrics not_good_enough{};
    not_good_enough.per_1s_permille = 6;
    c.update(not_good_enough, 1000);
    assert(c.state() == State::WeakLink);

    Metrics good{};
    good.per_1s_permille = 5;
    c.update(good, 2000);
    assert(c.state() == State::Recovery);
    c.update(good, 3500);
    assert(c.state() == State::Recovery);
    c.update(good, 4000);
    assert(c.state() == State::Good);
  }

  // Adaptive jitter is an explicit feature; RF state alone cannot increase it.
  {
    FeatureFlags flags = allAdaptiveFeatures();
    flags.adaptive_jitter = false;
    LinkController fixed_jitter({}, flags);
    Metrics burst{};
    burst.burst_max = 3;
    fixed_jitter.update(burst, 0);
    assert(fixed_jitter.state() == State::Burst);
    assert(fixed_jitter.actions().jitter_target_ms == 40);

    flags.adaptive_jitter = true;
    LinkController adaptive_jitter({}, flags);
    adaptive_jitter.update(burst, 0);
    assert(adaptive_jitter.state() == State::Burst);
    assert(adaptive_jitter.actions().jitter_target_ms == 60);
  }

  // Airtime overload still sheds deadline repair even when explicitly enabled.
  {
    LinkController c({}, allAdaptiveFeatures());
    Metrics m{};
    m.airtime_percent = 80;
    c.update(m, 0);
    assert(!c.actions().deadline_arq);
  }

  std::cout << "test_link_controller: PASS\n";
}
