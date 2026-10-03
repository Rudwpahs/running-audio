// C2 offline replay: Gate B per-frame outcome streams through the *firmware* estimator
// (firmware/common/pr1_channel_quality.hpp) and scheduler (pr1_afh.hpp), with the Gate C
// runtime's proposal / two-phase / activation rules re-implemented here.
//
//   c2_replay <stream.txt> [key=value ...]
//
// Model (approximation, stated in REPORT_GATE_C2.md):
//  * A static-map Gate B run observes every channel about every 40 frames. In the replay
//    frame f goes to the channel the adaptive schedule gives (current map, or the reserved
//    probe channel); its outcome is that of the Gate B frame nearest in time that was sent on
//    the same channel (ties -> earlier). This keeps each channel's time-varying loss and
//    cross-channel burst timing; observations can be reused when fewer channels are active.
//  * Runtime rules (pr1_fixed_link_runtime.hpp, Gate C): a proposal is made when no map is
//    pending and no proposal is in flight; map first (estimator activeMap() != current),
//    otherwise a probe of nextProbeChannel(). Commit after `relay_frames`; a map activates at
//    proposal + lead (600), a probe frame is proposal + lead/2. beginProbe() at commit.
//  * Optional runtime cap: map proposals no closer than map_min_interval_ms (0 = Gate C).
// Output: one JSON object on stdout.

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <vector>

#include "../../../firmware/common/pr1_afh.hpp"
#include "../../../firmware/common/pr1_channel_quality.hpp"
#include "../../../firmware/t3s3_sx1280_runtime/include/pr1_afh_runtime.hpp"

using namespace pr1;

namespace {


struct Params {
  quality::Config q{};
  std::uint32_t lead = 600;
  std::uint32_t relay_frames = 4;
  std::uint32_t map_min_interval_ms = 0;
};

bool setParam(Params& p, const std::string& k, long v) {
  auto& q = p.q;
  if (k == "fast_shift") q.alpha_fast_shift = static_cast<std::uint8_t>(v);
  else if (k == "slow_shift") q.alpha_slow_shift = static_cast<std::uint8_t>(v);
  else if (k == "suspect_q15") q.suspect_pdr_q15 = static_cast<std::uint16_t>(v);
  else if (k == "exclude_q15") q.exclude_pdr_q15 = static_cast<std::uint16_t>(v);
  else if (k == "recover_q15") q.recover_pdr_q15 = static_cast<std::uint16_t>(v);
  else if (k == "suspect_losses") q.suspect_losses = static_cast<std::uint8_t>(v);
  else if (k == "exclude_losses") q.exclude_losses = static_cast<std::uint8_t>(v);
  else if (k == "recover_successes") q.recover_successes = static_cast<std::uint8_t>(v);
  else if (k == "probe_init_ms") q.initial_probe_ms = static_cast<std::uint32_t>(v);
  else if (k == "probe_max_ms") q.max_probe_ms = static_cast<std::uint32_t>(v);
  else if (k == "min_active") q.minimum_active_channels = static_cast<std::uint8_t>(v);
  else if (k == "reinstate_successes") q.reinstate_probe_successes = static_cast<std::uint8_t>(v);
  else if (k == "exclude_slow_q15") q.exclude_slow_pdr_q15 = static_cast<std::uint16_t>(v);
  else if (k == "lead") p.lead = static_cast<std::uint32_t>(v);
  else if (k == "relay_frames") p.relay_frames = static_cast<std::uint32_t>(v);
  else if (k == "map_min_interval_ms") p.map_min_interval_ms = static_cast<std::uint32_t>(v);
  else return false;
  return true;
}

struct Stream {
  std::string run;
  std::uint64_t L0 = 0, L1 = 0;
  std::uint32_t period_us = 3000;
  std::uint32_t t0_ms = 0;
  std::vector<std::uint8_t> lost;  // per frame (index f - L0): 1 lost
};

bool load(const char* path, Stream* s) {
  FILE* fh = std::fopen(path, "r");
  if (fh == nullptr) return false;
  char line[256];
  std::vector<std::pair<std::uint64_t, int>> lost;
  while (std::fgets(line, sizeof(line), fh) != nullptr) {
    if (line[0] == '#') {
      char run[160] = {};
      unsigned long long L0 = 0, L1 = 0;
      unsigned period = 0, t0 = 0;
      if (std::sscanf(line, "# run=%159s L0=%llu L1=%llu period_us=%u t0_ms=%u", run, &L0, &L1, &period, &t0) == 5) {
        s->run = run; s->L0 = L0; s->L1 = L1; s->period_us = period; s->t0_ms = t0;
      }
      continue;
    }
    unsigned long long L = 0; int k = 0;
    if (std::sscanf(line, "%llu %d", &L, &k) == 2) lost.emplace_back(L, k);
  }
  std::fclose(fh);
  if (s->L1 < s->L0) return false;
  s->lost.assign(static_cast<std::size_t>(s->L1 - s->L0 + 1), 0);
  for (auto& [L, k] : lost) {
    if (L >= s->L0 && L <= s->L1) s->lost[static_cast<std::size_t>(L - s->L0)] = 1;
  }
  return true;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc < 2) {
    std::fprintf(stderr, "usage: c2_replay <stream.txt> [key=value ...]\n");
    return 2;
  }
  Params p{};
  for (int i = 2; i < argc; ++i) {
    const char* eq = std::strchr(argv[i], '=');
    if (eq == nullptr || !setParam(p, std::string(argv[i], static_cast<std::size_t>(eq - argv[i])), std::strtol(eq + 1, nullptr, 10))) {
      std::fprintf(stderr, "bad param %s\n", argv[i]);
      return 2;
    }
  }
  Stream s{};
  if (!load(argv[1], &s)) {
    std::fprintf(stderr, "cannot load %s\n", argv[1]);
    return 2;
  }
  const std::size_t n = s.lost.size();

  // Gate B (static all-40) channel of every frame, and per-channel visit lists.
  // Exactly the firmware static config (seed, session id, map_version, all-40 map).
  const afh::ScheduleConfig base = runtime::afhrt::staticScheduleConfig();
  const afh::Scheduler static_sched(base);
  std::array<std::vector<std::uint32_t>, afh::kChannelCount> visits{};
  for (std::size_t i = 0; i < n; ++i) {
    visits[static_sched.channelForSequence(s.L0 + i)].push_back(static_cast<std::uint32_t>(i));
  }
  auto outcomeOn = [&](std::uint8_t ch, std::size_t i) -> bool {  // true = received
    const auto& v = visits[ch];
    if (v.empty()) return true;
    auto it = std::lower_bound(v.begin(), v.end(), static_cast<std::uint32_t>(i));
    std::size_t j;
    if (it == v.end()) j = v.back();
    else if (it == v.begin()) j = *it;
    else {
      const std::uint32_t after = *it, before = *(it - 1);
      j = (after - i) < (i - before) ? after : before;
    }
    return s.lost[j] == 0;
  };

  quality::Estimator est(p.q);
  afh::Scheduler sched(base);
  enum class Prop { None, Map, Probe };
  Prop prop = Prop::None;
  std::uint64_t prop_frame = 0, prop_target = 0;
  std::uint64_t prop_bits = 0;
  std::uint8_t prop_ch = 0;
  bool probe_valid = false;
  std::uint64_t probe_at = 0;
  std::uint8_t probe_ch = 0;
  std::uint32_t last_map_prop_ms = 0;
  bool any_map_prop = false;

  // Metrics.
  std::uint64_t lost_sim = 0, frames = 0, probes = 0, probe_ok = 0, map_props = 0, activations = 0;
  std::uint64_t exclusions = 0, reinclusions = 0, isolated_exclusions = 0, single_loss_exclusions = 0,
                at_floor_frames = 0;
  std::uint64_t data_obs = 0;
  std::uint8_t min_active = 40;
  std::vector<std::uint8_t> active_after;  // active count after each activation
  std::vector<std::uint32_t> activation_ms;
  std::array<std::uint32_t, afh::kChannelCount> recent{};  // last 32 data outcomes (bit = loss)
  std::array<std::uint32_t, afh::kChannelCount> ch_lost_b{}, ch_seen_b{}, ch_excl{};
  for (std::size_t i = 0; i < n; ++i) {
    const std::uint8_t c = static_sched.channelForSequence(s.L0 + i);
    ++ch_seen_b[c];
    ch_lost_b[c] += s.lost[i];
  }

  for (std::size_t i = 0; i < n; ++i) {
    const std::uint64_t f = s.L0 + i;
    const std::uint32_t now_ms = s.t0_ms + static_cast<std::uint32_t>((i * s.period_us) / 1000U);
    // Pending map activates at its frame (both sides, same frame by construction).
    if (sched.pending().valid && f >= sched.pending().activation_sequence) {
      sched.applyPendingIfDue(f);
      ++activations;
      const std::uint8_t a = sched.current().map.activeCount();
      active_after.push_back(a);
      activation_ms.push_back(now_ms);
      if (a < min_active) min_active = a;
    }
    // Commit of an in-flight proposal after the relay round trip.
    if (prop != Prop::None && f >= prop_frame + p.relay_frames) {
      if (prop == Prop::Map) {
        afh::ChannelMap m{};
        m.bits = prop_bits;
        sched.stageMap(static_cast<std::uint16_t>(sched.current().map_version + 1U), m, prop_target);
      } else if (est.beginProbe(prop_ch, now_ms)) {
        probe_valid = true;
        probe_at = prop_target;
        probe_ch = prop_ch;
      }
      prop = Prop::None;
    }
    // Channel and outcome of this frame.
    const bool is_probe = probe_valid && f == probe_at;
    const std::uint8_t ch = is_probe ? probe_ch : sched.channelForSequence(f);
    const bool ok = outcomeOn(ch, i);
    ++frames;
    if (!ok) ++lost_sim;
    if (est.activeCount() <= p.q.minimum_active_channels) ++at_floor_frames;
    if (is_probe) {
      ++probes;
      if (ok) ++probe_ok;
      const bool was = est.channel(ch).state == quality::ChannelState::Probe;
      est.observeProbe(ch, ok, now_ms);
      if (was && est.channel(ch).state == quality::ChannelState::Active) ++reinclusions;
      probe_valid = false;
    } else {
      ++data_obs;
      recent[ch] = (recent[ch] << 1U) | (ok ? 0U : 1U);
      const auto before = est.channel(ch).state;
      est.observeData(ch, ok, now_ms);
      if (before != quality::ChannelState::Excluded && est.channel(ch).state == quality::ChannelState::Excluded) {
        ++exclusions;
        ++ch_excl[ch];
        if (__builtin_popcount(recent[ch] & 0xFFU) <= 1) ++isolated_exclusions;
        if (__builtin_popcount(recent[ch]) <= 1) ++single_loss_exclusions;
      }
    }
    if (probe_valid && f > probe_at) probe_valid = false;
    // Proposal (RX idle loop, one per frame at most).
    if (prop == Prop::None && !sched.pending().valid) {
      const afh::ChannelMap desired = est.activeMap();
      const bool cap_ok = p.map_min_interval_ms == 0 || !any_map_prop ||
                          now_ms - last_map_prop_ms >= p.map_min_interval_ms;
      if (desired.bits != sched.current().map.bits && desired.isValid()) {
        if (cap_ok) {
          prop = Prop::Map;
          prop_frame = f;
          prop_target = f + p.lead;
          prop_bits = desired.bits;
          ++map_props;
          last_map_prop_ms = now_ms;
          any_map_prop = true;
        }
      } else if (!probe_valid) {
        std::uint8_t pc = 0;
        if (est.nextProbeChannel(now_ms, &pc)) {
          prop = Prop::Probe;
          prop_frame = f;
          prop_target = f + p.lead / 2U;
          prop_ch = pc;
        }
      }
    }
  }

  // Oscillation: an activation that raises the active count followed (next activation)
  // by one that lowers it, or vice versa.
  std::uint64_t reversals = 0;
  for (std::size_t k = 2; k < active_after.size(); ++k) {
    const int d1 = static_cast<int>(active_after[k - 1]) - static_cast<int>(active_after[k - 2]);
    const int d2 = static_cast<int>(active_after[k]) - static_cast<int>(active_after[k - 1]);
    if ((d1 > 0 && d2 < 0) || (d1 < 0 && d2 > 0)) ++reversals;
  }
  std::uint64_t lost_b = 0;
  for (auto v : s.lost) lost_b += v;
  // Do exclusions target the channels that were bad in the Gate B data?
  std::vector<std::pair<double, std::uint32_t>> rank;
  for (unsigned c = 0; c < afh::kChannelCount; ++c) {
    rank.emplace_back(ch_seen_b[c] ? static_cast<double>(ch_lost_b[c]) / ch_seen_b[c] : 0.0, ch_excl[c]);
  }
  std::sort(rank.begin(), rank.end(), [](auto& a, auto& b) { return a.first > b.first; });
  std::uint32_t excl_worst8 = 0, excl_total = 0;
  for (std::size_t k = 0; k < rank.size(); ++k) {
    excl_total += rank[k].second;
    if (k < 8) excl_worst8 += rank[k].second;
  }
  const double dur_s = static_cast<double>(n) * s.period_us / 1e6;
  std::printf(
      "{\"run\":\"%s\",\"frames\":%llu,\"lost_b\":%llu,\"lost_sim\":%llu,\"loss_b_pct\":%.4f,\"loss_sim_pct\":%.4f,"
      "\"map_proposals\":%llu,\"activations\":%llu,\"activations_per_min\":%.2f,\"reversals\":%llu,"
      "\"exclusions\":%llu,\"isolated_exclusions\":%llu,\"single_loss_exclusions\":%llu,\"reinclusions\":%llu,\"probes\":%llu,\"probe_ok\":%llu,"
      "\"min_active\":%u,\"final_active\":%u,\"floor_frac\":%.4f,\"excl_on_worst8\":%u,\"excl_total\":%u,"
      "\"duration_s\":%.1f}\n",
      s.run.c_str(), static_cast<unsigned long long>(frames), static_cast<unsigned long long>(lost_b),
      static_cast<unsigned long long>(lost_sim), 100.0 * lost_b / n, 100.0 * lost_sim / frames,
      static_cast<unsigned long long>(map_props), static_cast<unsigned long long>(activations),
      activations * 60.0 / dur_s, static_cast<unsigned long long>(reversals),
      static_cast<unsigned long long>(exclusions), static_cast<unsigned long long>(isolated_exclusions),
      static_cast<unsigned long long>(single_loss_exclusions),
      static_cast<unsigned long long>(reinclusions), static_cast<unsigned long long>(probes),
      static_cast<unsigned long long>(probe_ok), static_cast<unsigned>(min_active),
      static_cast<unsigned>(sched.current().map.activeCount()), static_cast<double>(at_floor_frames) / frames,
      excl_worst8, excl_total, dur_s);
  return 0;
}
