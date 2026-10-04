// Gate C3 estimator knobs (issue #52): exclusion memory / strike backoff, neighbour
// corroboration, probe window and probation. Every knob defaults to off; the defaults must
// reproduce Gate C/C2 behaviour exactly.
//
// These tests use assert(): never build this file with -DNDEBUG.
#ifdef NDEBUG
#error "test_channel_quality_c3 must be built without NDEBUG"
#endif
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <iostream>

#ifdef PR1_C3_GOLDEN_HEADER  // used once to compute the golden hashes from the pre-C3 header
#include PR1_C3_GOLDEN_HEADER
#else
#include "../firmware/common/pr1_channel_quality.hpp"
#endif

using pr1::quality::ChannelState;
using pr1::quality::Config;
using pr1::quality::Estimator;

namespace {

Config c2() {
  Config c{};
  c.exclude_pdr_q15 = 22937;       // 0.70
  c.exclude_slow_pdr_q15 = 31129;  // 0.95
  c.reinstate_probe_successes = 3;
  c.initial_probe_ms = 3200;
  c.max_probe_ms = 25600;
  return c;
}

// Deterministic pseudo-random drive of every public entry point; returns a hash of all
// observable state (fields that exist before and after C3). The golden values below were
// produced by compiling this function against the Gate C2 header (commit afacec3).
// As the firmware does during the 600-frame map lead, data outcomes are also fed for
// channels that are Excluded or in Probe.
std::uint64_t traceHash(const Config& cfg, std::uint32_t seed) {
  Estimator e{cfg};
  std::uint64_t h = 1469598103934665603ULL;
  auto mix = [&h](std::uint64_t v) { h = (h ^ v) * 1099511628211ULL; };
  std::uint32_t x = seed, t = 1000;
  auto rnd = [&x]() { x = x * 1664525U + 1013904223U; return x >> 8U; };
  std::uint32_t data_while_out = 0;
  for (int step = 0; step < 200000; ++step) {
    t += 3;
    const std::uint8_t c = static_cast<std::uint8_t>(rnd() % 40U);
    // Channels 20..31 lose ~8 %, the rest ~0.5 %: a band-limited interferer.
    const std::uint32_t loss_pm = (c >= 20 && c <= 31) ? 80U : 5U;
    const bool ok = rnd() % 1000U >= loss_pm;
    const auto st = e.channel(c).state;
    if (st == ChannelState::Excluded || st == ChannelState::Probe) {
      if (rnd() % 3U != 0U) {  // data frame on a channel the estimator already dropped
        e.observeData(c, ok, t);
        ++data_while_out;
      }
      std::uint8_t pc = 0;
      if (e.nextProbeChannel(t, &pc)) {
        mix(pc);
        if (e.beginProbe(pc, t)) mix(e.observeProbe(pc, rnd() % 1000U >= loss_pm, t + 1) ? 1U : 2U);
      }
    } else {
      e.observeData(c, ok, t);
    }
    if (step % 97 == 0) {
      mix(e.activeMap().bits);
      mix(e.activeCount());
      for (std::uint8_t k = 0; k < 40U; ++k) {
        const auto& s = e.channel(k);
        mix(s.pdr_fast_q15); mix(s.pdr_slow_q15); mix(s.consecutive_losses); mix(s.consecutive_successes);
        mix(s.last_seen_ms); mix(s.last_probe_ms); mix(s.probe_failure_exp);
        mix(static_cast<std::uint8_t>(s.state)); mix(s.probe_history_bits); mix(s.probe_history_count);
        mix(static_cast<std::uint32_t>(e.reprobeScore(k, t)));
      }
    }
  }
  mix(data_while_out);
  if (data_while_out < 1000U) return 0;  // the trace must exercise the excluded-data path
  return h;
}

// Produced by building this file with -DPR1_C3_GOLDEN_HEADER="<afacec3 pr1_channel_quality.hpp>"
// (git show afacec3:firmware/common/pr1_channel_quality.hpp), -I firmware/common.
constexpr std::uint64_t kGoldenDefault = 0x25c3cf156c160eceULL;
constexpr std::uint64_t kGoldenC2 = 0xc299ce2126e24e3cULL;

#ifndef PR1_C3_GOLDEN_HEADER
using pr1::quality::ExclusionReason;

constexpr std::uint16_t kDirect082 = 26869;  // 0.82: one own loss

// The Gate C3 candidate family: C2 values + strike memory + +/-2 neighbour corroboration.
Config c3sel() {
  Config c = c2();
  c.strike_backoff_max_shift = 2;
  c.strike_probe_ms = 25600;
  c.strike_max_probe_ms = 102400;
  c.strike_decay_ms = 120000;
  c.neighbor_radius = 2;
  c.neighbor_min_bad = 2;
  c.neighbor_bad_slow_q15 = 0;
  c.neighbor_direct_fast_q15 = kDirect082;
  return c;
}

void defaultsUnchanged() {
  const Config d{};
  assert(d.strike_backoff_max_shift == 0 && d.strike_probe_ms == 0 && d.strike_max_probe_ms == 0);
  assert(d.strike_decay_ms == 0);
  assert(d.neighbor_radius == 0 && d.neighbor_direct_fast_q15 == 0);
  assert(d.reinstate_probe_window == 3 && d.probation_visits == 0);
  assert(d.reinstate_reset_pdr == 0);
  // Bit-identical state trajectories against the pre-C3 (Gate C2) estimator.
  assert(kGoldenDefault != 0 && kGoldenC2 != 0);
  assert(traceHash(Config{}, 1U) == kGoldenDefault);
  assert(traceHash(c2(), 2U) == kGoldenC2);
}

void excludeNow(Estimator& e, std::uint8_t c, std::uint32_t& t) {
  for (int i = 0; i < 4; ++i) e.observeData(c, false, t += 3);
  assert(e.channel(c).state == ChannelState::Excluded);
}

void reincludeWithProbes(Estimator& e, std::uint8_t c, std::uint32_t& t, std::uint32_t* waited) {
  const std::uint32_t t0 = t;
  while (e.channel(c).state != ChannelState::Active) {
    t += 100;
    if (e.beginProbe(c, t)) e.observeProbe(c, true, t);
  }
  *waited = t - t0;
}

void strikeBackoffGrowsAndDecays() {
  // Slow-EWMA rule off here: with it, a channel re-included after 4 straight losses is
  // re-excluded on its second data visit (stale slow EWMA), which would hide the decay.
  Config c = c2();
  c.exclude_slow_pdr_q15 = 0;
  c.strike_backoff_max_shift = 3;
  c.strike_probe_ms = 6400;
  c.strike_max_probe_ms = 204800;
  c.strike_decay_ms = 120000;
  Estimator e{c};
  std::uint32_t t = 1000, w1 = 0, w2 = 0, w3 = 0;
  excludeNow(e, 4, t);
  assert(e.channel(4).strikes == 1);
  reincludeWithProbes(e, 4, t, &w1);  // 1 strike: 3 probes at 3.2 s
  assert(w1 >= 3 * 3200 && w1 < 3 * 3200 + 400);
  excludeNow(e, 4, t);
  assert(e.channel(4).strikes == 2);   // the strike survived re-inclusion
  reincludeWithProbes(e, 4, t, &w2);  // 2 strikes: 6.4 s * 2
  assert(w2 >= 3 * 12800 && w2 < 3 * 12800 + 400);
  excludeNow(e, 4, t);
  reincludeWithProbes(e, 4, t, &w3);  // 3 strikes: 6.4 s * 4
  assert(w3 >= 3 * 25600 && w3 < 3 * 25600 + 400);
  // Forgiveness: after 120 s of clean inclusion one strike is removed (per 120 s).
  assert(e.channel(4).strikes == 3);
  for (int i = 0; i < 1000; ++i) e.observeData(4, true, t += 125);  // 125 s
  assert(e.channel(4).strikes == 2);
  for (int i = 0; i < 2000; ++i) e.observeData(4, true, t += 125);  // + 250 s
  assert(e.channel(4).strikes == 0);

  // Same sequence with the knob off: the interval never grows.
  Config off_cfg = c2();
  off_cfg.exclude_slow_pdr_q15 = 0;
  Estimator off{off_cfg};
  t = 1000;
  for (int k = 0; k < 3; ++k) {
    excludeNow(off, 4, t);
    reincludeWithProbes(off, 4, t, &w1);
    assert(w1 < 3 * 3200 + 400);
  }
}

// strike_max_probe_ms replaces max_probe_ms for channels with >= 2 strikes, also when lower.
void strikeCapMayBeLowerThanMaxProbe() {
  Config c = c2();
  c.exclude_slow_pdr_q15 = 0;
  c.strike_backoff_max_shift = 3;
  c.strike_probe_ms = 6400;
  c.strike_max_probe_ms = 4000;  // < max_probe_ms (25600)
  Estimator e{c};
  std::uint32_t t = 1000, w = 0;
  excludeNow(e, 4, t);
  reincludeWithProbes(e, 4, t, &w);
  excludeNow(e, 4, t);
  assert(e.channel(4).strikes == 2);
  reincludeWithProbes(e, 4, t, &w);  // 6.4 s * 2 capped at 4.0 s
  assert(w >= 3 * 4000 && w < 3 * 4000 + 400);
  // 0 keeps max_probe_ms as the cap.
  c.strike_max_probe_ms = 0;
  c.strike_probe_ms = 20000;
  Estimator k{c};
  t = 1000;
  excludeNow(k, 4, t);
  reincludeWithProbes(k, 4, t, &w);
  excludeNow(k, 4, t);
  reincludeWithProbes(k, 4, t, &w);  // 20 s * 2 capped at max_probe_ms = 25.6 s
  assert(w >= 3 * 25600 && w < 3 * 25600 + 400);
}

// Decision: strikes are forgiven only for clean Active time. A channel that is kept in the
// map as Suspect because of the minimum-active floor earns nothing; any loss restarts the clock.
void strikesDoNotDecayWhileSuspectAtFloor() {
  Config c = c2();
  c.exclude_slow_pdr_q15 = 0;
  c.strike_backoff_max_shift = 2;
  c.strike_probe_ms = 6400;
  c.strike_decay_ms = 120000;
  Estimator e{c};
  std::uint32_t t = 1000, w = 0;
  excludeNow(e, 6, t);
  reincludeWithProbes(e, 6, t, &w);
  excludeNow(e, 6, t);
  reincludeWithProbes(e, 6, t, &w);
  assert(e.channel(6).strikes == 2 && e.activeCount() == 40);
  c.minimum_active_channels = 40;  // the floor now forbids every exclusion
  e.setConfig(c);
  // 400 s of 50 % loss: permanently Suspect, never excluded, never forgiven.
  for (int i = 0; i < 3200; ++i) {
    e.observeData(6, (i & 1) != 0, t += 125);
    assert(e.channel(6).state == ChannelState::Suspect || i == 0);
  }
  assert(e.channel(6).state == ChannelState::Suspect);
  assert(e.channel(6).strikes == 2);
  // An Active channel with one loss per 100 s is not forgiven either (clock restarts).
  Estimator a{c};
  c.minimum_active_channels = 12;
  a.setConfig(c);
  t = 1000;
  excludeNow(a, 6, t);
  reincludeWithProbes(a, 6, t, &w);
  assert(a.channel(6).strikes == 1);
  for (int i = 1; i <= 4000; ++i) a.observeData(6, i % 800 != 0, t += 125);  // 500 s, loss / 100 s
  assert(a.channel(6).state != ChannelState::Excluded);
  assert(a.channel(6).strikes == 1);
  for (int i = 0; i < 1000; ++i) a.observeData(6, true, t += 125);  // 125 s clean
  assert(a.channel(6).strikes == 0);
}

void neighbourNeedsDirectEvidence() {
  Config c = c2();
  c.neighbor_radius = 2;
  c.neighbor_min_bad = 2;
  c.neighbor_direct_fast_q15 = kDirect082;
  Estimator e{c};
  std::uint32_t t = 1000;
  excludeNow(e, 9, t);
  excludeNow(e, 11, t);
  // Untouched channel 10 between two excluded neighbours: clean visits never exclude it.
  for (int i = 0; i < 500; ++i) e.observeData(10, true, t += 3);
  assert(e.channel(10).state == ChannelState::Active);
  // One loss of its own + 2 bad neighbours -> excluded, by the neighbour rule, on the loss.
  e.observeData(10, false, t += 3);
  assert(e.channel(10).state == ChannelState::Excluded);
  assert(e.channel(10).last_exclusion_reason == ExclusionReason::Neighbor);
  assert(e.channel(10).losses_since_inclusion == 1);
  // One loss with only 1 bad neighbour within +/-2 -> not excluded (C2 rules apply).
  for (int i = 0; i < 20; ++i) e.observeData(30, true, t += 3);
  excludeNow(e, 31, t);
  e.observeData(30, false, t += 3);
  e.observeData(30, true, t += 3);
  assert(e.channel(30).state != ChannelState::Excluded);
  // A loss while the neighbours were still good, then the neighbours go bad: the following
  // received frames must not exclude (fast EWMA is still < 0.82 on the first of them).
  Estimator s{c};
  s.observeData(20, false, t += 3);
  assert(s.channel(20).state == ChannelState::Suspect);
  excludeNow(s, 19, t);
  excludeNow(s, 21, t);
  for (int i = 0; i < 50; ++i) {
    s.observeData(20, true, t += 3);
    assert(s.channel(20).state != ChannelState::Excluded);
  }
  s.observeData(20, false, t += 3);  // a new own loss: now it is excluded
  assert(s.channel(20).state == ChannelState::Excluded);
  assert(s.channel(20).last_exclusion_reason == ExclusionReason::Neighbor);
}

// Review repro: 9 and 11 excluded, 10 excluded by 4 losses, 3/3 probes OK, first data frame
// received. The old rule excluded 10 again on that received frame (reason Neighbor,
// strikes 2) because the fast EWMA was still below 0.82 from before the quarantine.
void neighbourNeverExcludesOnSuccessAfterReinclusion() {
  for (int data_while_excluded = 0; data_while_excluded <= 15; data_while_excluded += 15) {
    Estimator e{c3sel()};
    std::uint32_t t = 1000, w = 0;
    excludeNow(e, 9, t);
    excludeNow(e, 11, t);
    excludeNow(e, 10, t);
    // The firmware keeps feeding data outcomes for ~15 visits (600-frame lead) after exclusion.
    for (int i = 0; i < data_while_excluded; ++i) e.observeData(10, false, t += 120);
    reincludeWithProbes(e, 10, t, &w);
    assert(e.channel(10).state == ChannelState::Active);
    assert(e.channel(10).strikes == 1);
    assert(e.channel(10).losses_since_inclusion == 0);
    assert(e.channel(9).state == ChannelState::Excluded && e.channel(11).state == ChannelState::Excluded);
    assert(e.channel(10).pdr_fast_q15 < kDirect082);  // the stale EWMA the old rule relied on
    e.observeData(10, true, t += 3);                  // first data frame: success
    assert(e.channel(10).state != ChannelState::Excluded);
    assert(e.channel(10).strikes == 1);
    // No number of received frames may produce a neighbour exclusion. (With the C2 values the
    // stale *slow* EWMA rule still re-excludes it, reason SlowPdr: that is C2 behaviour.)
    for (int i = 0; i < 300; ++i) {
      e.observeData(10, true, t += 3);
      if (e.channel(10).state == ChannelState::Excluded) {
        assert(e.channel(10).last_exclusion_reason != ExclusionReason::Neighbor);
        break;
      }
    }
  }
  // Without the stale slow-EWMA rule, or with the EWMAs reset at re-inclusion, the channel
  // simply stays in the map until it loses a frame itself.
  for (int variant = 0; variant < 2; ++variant) {
    Config c = c3sel();
    if (variant == 0) c.exclude_slow_pdr_q15 = 0; else c.reinstate_reset_pdr = 1;
    Estimator e{c};
    std::uint32_t t = 1000, w = 0;
    excludeNow(e, 9, t);
    excludeNow(e, 11, t);
    excludeNow(e, 10, t);
    reincludeWithProbes(e, 10, t, &w);
    if (variant == 1) assert(e.channel(10).pdr_fast_q15 == 32767 && e.channel(10).pdr_slow_q15 == 32767);
    for (int i = 0; i < 300; ++i) {
      e.observeData(10, true, t += 3);
      assert(e.channel(10).state != ChannelState::Excluded);
    }
    assert(e.channel(10).strikes == 1 && e.channel(10).losses_since_inclusion == 0);
    e.observeData(10, false, t += 3);  // one new own loss + 2 excluded neighbours
    assert(e.channel(10).state == ChannelState::Excluded);
    assert(e.channel(10).last_exclusion_reason == ExclusionReason::Neighbor);
    assert(e.channel(10).strikes == 2);
  }
}

// A direct threshold that needs two own losses (0.60 < 0.75) cannot be met by a stale EWMA
// plus one new loss.
void neighbourDirectThresholdCountsLossesSinceInclusion() {
  Config c = c2();
  c.exclude_slow_pdr_q15 = 0;
  c.exclude_pdr_q15 = 0;  // fast-PDR rule off so that only the neighbour rule can act here
  c.neighbor_radius = 2;
  c.neighbor_min_bad = 2;
  c.neighbor_direct_fast_q15 = 19660;  // 0.60: two own losses from a clean EWMA
  Estimator e{c};
  std::uint32_t t = 1000, w = 0;
  excludeNow(e, 9, t);
  excludeNow(e, 11, t);
  excludeNow(e, 10, t);
  reincludeWithProbes(e, 10, t, &w);
  e.observeData(10, false, t += 3);  // fast EWMA 0.53 < 0.60, but only 1 loss since inclusion
  assert(e.channel(10).pdr_fast_q15 < 19660);
  assert(e.channel(10).state == ChannelState::Suspect);
  // Fresh channel: first loss no, second (non-consecutive) loss yes.
  excludeNow(e, 14, t);
  excludeNow(e, 16, t);
  e.observeData(15, false, t += 3);
  assert(e.channel(15).state == ChannelState::Suspect);
  e.observeData(15, true, t += 3);
  e.observeData(15, false, t += 3);  // 2 losses, but fast EWMA 0.609 >= 0.60
  assert(e.channel(15).state == ChannelState::Suspect);
  e.observeData(15, false, t += 3);  // fast 0.457 < 0.60 and 3 losses since inclusion
  assert(e.channel(15).state == ChannelState::Excluded);
  assert(e.channel(15).last_exclusion_reason == ExclusionReason::Neighbor);
  // The re-included channel 10 needs a second new loss as well.
  e.observeData(10, false, t += 3);
  assert(e.channel(10).state == ChannelState::Excluded);
  assert(e.channel(10).last_exclusion_reason == ExclusionReason::Neighbor);
}

void neighbourEdgeChannels() {
  Config c = c2();
  c.neighbor_radius = 2;
  c.neighbor_min_bad = 2;
  c.neighbor_direct_fast_q15 = kDirect082;
  {
    Estimator e{c};
    std::uint32_t t = 1000;
    excludeNow(e, 1, t);
    e.observeData(0, false, t += 3);  // only 1 in-range bad neighbour
    assert(e.channel(0).state == ChannelState::Suspect);
    for (int i = 0; i < 20; ++i) e.observeData(0, true, t += 3);
    assert(e.channel(0).state == ChannelState::Active);
    excludeNow(e, 2, t);
    e.observeData(0, false, t += 3);  // neighbours 1 and 2 (there is no -1 / -2)
    assert(e.channel(0).state == ChannelState::Excluded);
    assert(e.channel(0).last_exclusion_reason == ExclusionReason::Neighbor);
  }
  {
    Estimator e{c};
    std::uint32_t t = 1000;
    excludeNow(e, 38, t);
    e.observeData(39, false, t += 3);
    assert(e.channel(39).state == ChannelState::Suspect);
    for (int i = 0; i < 20; ++i) e.observeData(39, true, t += 3);
    assert(e.channel(39).state == ChannelState::Active);
    excludeNow(e, 37, t);
    e.observeData(39, false, t += 3);  // neighbours 37 and 38 (there is no 40 / 41)
    assert(e.channel(39).state == ChannelState::Excluded);
    assert(e.channel(39).last_exclusion_reason == ExclusionReason::Neighbor);
  }
  {  // radius 1 at the edges: the single existing neighbour
    Config r1 = c;
    r1.neighbor_radius = 1;
    r1.neighbor_min_bad = 1;
    Estimator e{r1};
    std::uint32_t t = 1000;
    excludeNow(e, 2, t);  // not adjacent to 0
    e.observeData(0, false, t += 3);
    assert(e.channel(0).state == ChannelState::Suspect);
    excludeNow(e, 38, t);
    e.observeData(39, false, t += 3);
    assert(e.channel(39).state == ChannelState::Excluded);
  }
}

void neighbourBadSlowAndProbeStates() {
  Config c = c2();
  c.neighbor_radius = 2;
  c.neighbor_min_bad = 2;
  c.neighbor_direct_fast_q15 = kDirect082;
  // (a) neighbor_bad_slow_q15: Active/Suspect neighbours with a low slow EWMA count as bad.
  for (int with_slow = 0; with_slow < 2; ++with_slow) {
    Config k = c;
    k.neighbor_bad_slow_q15 = with_slow ? 31784 : 0;  // 0.97
    Estimator e{k};
    std::uint32_t t = 1000;
    e.observeData(14, false, t += 3);  // slow 0.969 < 0.97, still in the map
    e.observeData(16, false, t += 3);
    assert(e.channel(14).state == ChannelState::Suspect && e.channel(16).state == ChannelState::Suspect);
    for (int i = 0; i < 100; ++i) e.observeData(15, true, t += 3);  // never on a received frame
    assert(e.channel(15).state == ChannelState::Active);
    e.observeData(15, false, t += 3);
    if (with_slow) {
      assert(e.channel(15).state == ChannelState::Excluded);
      assert(e.channel(15).last_exclusion_reason == ExclusionReason::Neighbor);
    } else {
      assert(e.channel(15).state == ChannelState::Suspect);
    }
  }
  // (b) neighbours in Probe state count like Excluded ones.
  Estimator e{c};
  std::uint32_t t = 1000;
  excludeNow(e, 21, t);
  excludeNow(e, 23, t);
  t += 4000;
  assert(e.beginProbe(21, t) && e.beginProbe(23, t));
  assert(e.channel(21).state == ChannelState::Probe && e.channel(23).state == ChannelState::Probe);
  e.observeData(22, true, t += 3);
  assert(e.channel(22).state == ChannelState::Active);
  e.observeData(22, false, t += 3);
  assert(e.channel(22).state == ChannelState::Excluded);
  assert(e.channel(22).last_exclusion_reason == ExclusionReason::Neighbor);
}

void neighbourConfigIsNeutralised() {
  std::uint32_t t = 1000;
  {  // radius is clamped to 2: bad channels at distance 3 do not count.
    Config c = c2();
    c.neighbor_radius = 5;
    c.neighbor_min_bad = 2;
    c.neighbor_direct_fast_q15 = kDirect082;
    Estimator e{c};
    excludeNow(e, 2, t);
    excludeNow(e, 8, t);
    e.observeData(5, false, t += 3);
    assert(e.channel(5).state == ChannelState::Suspect);
    Estimator f{c};
    excludeNow(f, 3, t);
    excludeNow(f, 7, t);
    f.observeData(5, false, t += 3);
    assert(f.channel(5).state == ChannelState::Excluded);
  }
  {  // neighbor_min_bad = 0 is treated as 1: no bad neighbour, no neighbour exclusion.
    Config c = c2();
    c.neighbor_radius = 2;
    c.neighbor_min_bad = 0;
    c.neighbor_direct_fast_q15 = kDirect082;
    Estimator e{c};
    e.observeData(5, false, t += 3);
    assert(e.channel(5).state == ChannelState::Suspect);
    excludeNow(e, 7, t);
    e.observeData(6, false, t += 3);
    assert(e.channel(6).state == ChannelState::Excluded);
  }
  {  // a direct threshold above 1.0 ("always") still needs an own loss on this frame.
    Config c = c2();
    c.neighbor_radius = 2;
    c.neighbor_min_bad = 2;
    c.neighbor_direct_fast_q15 = 65535;
    Estimator e{c};
    excludeNow(e, 9, t);
    excludeNow(e, 11, t);
    for (int i = 0; i < 500; ++i) e.observeData(10, true, t += 3);
    assert(e.channel(10).state == ChannelState::Active);
    e.observeData(10, false, t += 3);
    assert(e.channel(10).state == ChannelState::Excluded);
  }
  {  // an unreachable threshold switches the rule off; setConfig re-derives it.
    Config c = c2();
    c.neighbor_radius = 2;
    c.neighbor_min_bad = 2;
    c.neighbor_direct_fast_q15 = 1;
    Estimator e{c};
    excludeNow(e, 9, t);
    excludeNow(e, 11, t);
    e.observeData(10, false, t += 3);
    assert(e.channel(10).state == ChannelState::Suspect);
    for (int i = 0; i < 20; ++i) e.observeData(10, true, t += 3);
    c.neighbor_direct_fast_q15 = kDirect082;
    e.setConfig(c);
    e.observeData(10, false, t += 3);
    assert(e.channel(10).state == ChannelState::Excluded);
  }
}

void probeWindowSix() {
  Config c = c2();
  c.reinstate_probe_window = 6;
  c.reinstate_probe_successes = 6;
  Estimator e{c};
  std::uint32_t t = 1000;
  excludeNow(e, 7, t);
  for (int k = 0; k < 5; ++k) {
    t += 30000;
    assert(e.beginProbe(7, t));
    e.observeProbe(7, true, t);
    assert(e.channel(7).state == ChannelState::Excluded);
  }
  t += 30000;
  assert(e.beginProbe(7, t));
  e.observeProbe(7, true, t);
  assert(e.channel(7).state == ChannelState::Active);
}

// reinstate_probe_window is clamped to 1..8.
void probeWindowClamp() {
  auto probesToReinstate = [](std::uint8_t window, std::uint8_t need) {
    Config c = c2();
    c.reinstate_probe_window = window;
    c.reinstate_probe_successes = need;
    Estimator e{c};
    std::uint32_t t = 1000;
    for (int i = 0; i < 4; ++i) e.observeData(7, false, t += 3);
    int n = 0;
    while (e.channel(7).state != ChannelState::Active && n < 50) {
      t += 30000;
      if (!e.beginProbe(7, t)) return -1;
      e.observeProbe(7, true, t);
      ++n;
    }
    return n;
  };
  assert(probesToReinstate(0, 1) == 1);     // 0 -> window 1
  assert(probesToReinstate(1, 1) == 1);
  assert(probesToReinstate(8, 8) == 8);
  assert(probesToReinstate(200, 8) == 8);   // 200 -> window 8 (history is 8 bits)
  assert(probesToReinstate(200, 3) == 8);   // the window must fill before reinstatement
  assert(probesToReinstate(200, 9) == 50);  // more successes than the clamped window: never
}

void probationReexcludesOnFirstLoss() {
  Config c = c2();
  c.exclude_slow_pdr_q15 = 0;  // see strikeBackoffGrowsAndDecays
  c.probation_visits = 32;
  Estimator e{c};
  std::uint32_t t = 1000, w = 0;
  excludeNow(e, 5, t);
  reincludeWithProbes(e, 5, t, &w);
  for (int i = 0; i < 10; ++i) e.observeData(5, true, t += 3);
  e.observeData(5, false, t += 3);  // loss during probation
  assert(e.channel(5).state == ChannelState::Excluded);
  assert(e.channel(5).last_exclusion_reason == ExclusionReason::Probation);
  assert(e.channel(5).strikes == 2);
  // After probation has passed a single isolated loss does not exclude.
  reincludeWithProbes(e, 5, t, &w);
  for (int i = 0; i < 200; ++i) e.observeData(5, true, t += 3);
  e.observeData(5, false, t += 3);
  for (int i = 0; i < 10; ++i) e.observeData(5, true, t += 3);
  assert(e.channel(5).state != ChannelState::Excluded);
}

void floorRespected() {
  Config c = c2();
  c.probation_visits = 32;
  c.neighbor_radius = 1;
  c.neighbor_direct_fast_q15 = kDirect082;
  c.minimum_active_channels = 38;
  Estimator e{c};
  std::uint32_t t = 1000;
  excludeNow(e, 1, t);
  excludeNow(e, 3, t);
  assert(e.activeCount() == 38);
  e.observeData(2, false, t += 3);  // neighbour rule would fire, floor forbids it
  assert(e.channel(2).state != ChannelState::Excluded);
}
#endif

}  // namespace

int main() {
#ifdef PR1_C3_GOLDEN_HEADER
  std::printf("0x%016llxULL 0x%016llxULL\n", static_cast<unsigned long long>(traceHash(Config{}, 1U)),
              static_cast<unsigned long long>(traceHash(c2(), 2U)));
  (void)kGoldenDefault;
  (void)kGoldenC2;
#else
  defaultsUnchanged();
  strikeBackoffGrowsAndDecays();
  strikeCapMayBeLowerThanMaxProbe();
  strikesDoNotDecayWhileSuspectAtFloor();
  neighbourNeedsDirectEvidence();
  neighbourNeverExcludesOnSuccessAfterReinclusion();
  neighbourDirectThresholdCountsLossesSinceInclusion();
  neighbourEdgeChannels();
  neighbourBadSlowAndProbeStates();
  neighbourConfigIsNeutralised();
  probeWindowSix();
  probeWindowClamp();
  probationReexcludesOnFirstLoss();
  floorRespected();
  std::cout << "test_channel_quality_c3: PASS\n";
#endif
  return 0;
}
