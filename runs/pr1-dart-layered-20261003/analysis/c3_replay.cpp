// C3 offline replay: Gate B per-frame outcome streams through the *firmware* estimator
// (firmware/common/pr1_channel_quality.hpp, incl. the Gate C3 knobs) and scheduler
// (pr1_afh.hpp). Extends c2_replay.cpp; the outcome model and runtime proposal rules are
// identical (with all C3 knobs at their defaults it reproduces c2_replay bit for bit).
//
//   c3_replay <stream.txt>[+<stream2.txt>...] [key=value ...]
//
// Outcome model (as C2): frame f goes to the channel the adaptive schedule gives; its outcome
// is that of the static-map Gate B frame nearest in time on the same channel.
// Multi-segment input ("a.txt+b.txt", or "a.txt+a.txt+a.txt" = loop): the segments are
// concatenated in time; every segment keeps its own static all-40 channel sequence, so each
// channel's loss events stay attached to that channel. Used for the long (~100 s)
// convergence / recovery stress streams.
//
// Synthetic recovery stress (clean_top > 0): the clean_top channels with the highest Gate B
// loss rate (>= 2 losses) lose all their losses from frame clean_at_permille/1000 * N on.
// time-to-recover = from that frame until the channel is next in the *applied* map.
//
// Evidence accounting: "recent" (last 32 data outcomes) and "losses since inclusion" are per
// channel and are cleared at re-inclusion; data frames seen while the estimator already has the
// channel Excluded/Probe (the map lead) are not evidence for a later exclusion. Every exclusion
// is classified by reason and by the number of own losses since (re)inclusion (0 / 1).
// Convergence: map activations per bin_ms (default 300 s) bin.
//
// Runtime rules (pr1_fixed_link_runtime.hpp, Gate C): one proposal in flight, map before
// probe, commit after relay_frames, map activation at +lead, probe frame at +lead/2,
// optional map-proposal cap map_min_interval_ms.
// Output: one JSON object on stdout.

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
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
  std::uint32_t clean_top = 0;
  std::uint32_t clean_at_permille = 500;
  std::uint32_t known_bad_top = 8;
  std::uint32_t bin_ms = 300000;  // map-change rate bins (convergence)
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
  // C3 knobs
  else if (k == "strike_shift") q.strike_backoff_max_shift = static_cast<std::uint8_t>(v);
  else if (k == "strike_probe_ms") q.strike_probe_ms = static_cast<std::uint32_t>(v);
  else if (k == "strike_max_ms") q.strike_max_probe_ms = static_cast<std::uint32_t>(v);
  else if (k == "strike_decay_ms") q.strike_decay_ms = static_cast<std::uint32_t>(v);
  else if (k == "nb_radius") q.neighbor_radius = static_cast<std::uint8_t>(v);
  else if (k == "nb_min_bad") q.neighbor_min_bad = static_cast<std::uint8_t>(v);
  else if (k == "nb_bad_slow_q15") q.neighbor_bad_slow_q15 = static_cast<std::uint16_t>(v);
  else if (k == "nb_direct_fast_q15") q.neighbor_direct_fast_q15 = static_cast<std::uint16_t>(v);
  else if (k == "probe_window") q.reinstate_probe_window = static_cast<std::uint8_t>(v);
  else if (k == "probation") q.probation_visits = static_cast<std::uint8_t>(v);
  else if (k == "reset_pdr") q.reinstate_reset_pdr = static_cast<std::uint8_t>(v);
  else if (k == "bin_ms") p.bin_ms = static_cast<std::uint32_t>(v > 0 ? v : 300000);
  // runtime / harness
  else if (k == "lead") p.lead = static_cast<std::uint32_t>(v);
  else if (k == "relay_frames") p.relay_frames = static_cast<std::uint32_t>(v);
  else if (k == "map_min_interval_ms") p.map_min_interval_ms = static_cast<std::uint32_t>(v);
  else if (k == "clean_top") p.clean_top = static_cast<std::uint32_t>(v);
  else if (k == "clean_at_permille") p.clean_at_permille = static_cast<std::uint32_t>(v);
  else if (k == "known_bad_top") p.known_bad_top = static_cast<std::uint32_t>(v);
  else return false;
  return true;
}

struct Segment {
  std::string run;
  std::uint64_t L0 = 0, L1 = 0;
  std::uint32_t period_us = 3000;
  std::uint32_t t0_ms = 0;
  std::vector<std::uint8_t> lost;
};

bool load(const char* path, Segment* s) {
  FILE* fh = std::fopen(path, "r");
  if (fh == nullptr) return false;
  char line[256];
  std::vector<std::uint64_t> lost;
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
    if (std::sscanf(line, "%llu %d", &L, &k) == 2) lost.push_back(L);
  }
  std::fclose(fh);
  if (s->L1 < s->L0) return false;
  s->lost.assign(static_cast<std::size_t>(s->L1 - s->L0 + 1), 0);
  for (auto L : lost) {
    if (L >= s->L0 && L <= s->L1) s->lost[static_cast<std::size_t>(L - s->L0)] = 1;
  }
  return true;
}

std::string jsonList(const std::vector<double>& v) {
  std::string out = "[";
  char buf[32];
  for (std::size_t i = 0; i < v.size(); ++i) {
    std::snprintf(buf, sizeof(buf), "%s%.0f", i ? "," : "", v[i]);
    out += buf;
  }
  return out + "]";
}

}  // namespace

int main(int argc, char** argv) {
  if (argc < 2) {
    std::fprintf(stderr, "usage: c3_replay <stream.txt>[+<stream.txt>...] [key=value ...]\n");
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
  // Load segments.
  std::vector<Segment> segs;
  {
    std::string all = argv[1];
    std::size_t pos = 0;
    while (pos <= all.size()) {
      const std::size_t plus = all.find('+', pos);
      const std::string path = all.substr(pos, plus == std::string::npos ? std::string::npos : plus - pos);
      Segment sg{};
      if (!load(path.c_str(), &sg)) {
        std::fprintf(stderr, "cannot load %s\n", path.c_str());
        return 2;
      }
      segs.push_back(std::move(sg));
      if (plus == std::string::npos) break;
      pos = plus + 1;
    }
  }
  const Segment& s0 = segs.front();
  std::string run = s0.run;
  if (segs.size() > 1) {
    bool same = true;
    for (auto& sg : segs) same = same && sg.run == s0.run;
    run = same ? ("loop" + std::to_string(segs.size()) + "__" + s0.run) : ("stitch__" + s0.run);
  }

  // Static (Gate B) channel and outcome of every concatenated frame.
  const afh::ScheduleConfig base = runtime::afhrt::staticScheduleConfig();
  const afh::Scheduler static_sched(base);
  std::vector<std::uint8_t> lost, bch;
  for (auto& sg : segs) {
    for (std::size_t i = 0; i < sg.lost.size(); ++i) {
      lost.push_back(sg.lost[i]);
      bch.push_back(static_sched.channelForSequence(sg.L0 + i));
    }
  }
  const std::size_t n = lost.size();
  const std::uint32_t period_us = s0.period_us;

  std::array<std::uint32_t, afh::kChannelCount> ch_lost_b{}, ch_seen_b{};
  for (std::size_t i = 0; i < n; ++i) {
    ++ch_seen_b[bch[i]];
    ch_lost_b[bch[i]] += lost[i];
  }
  // Known-bad channels: top known_bad_top by Gate B loss rate with >= 2 losses.
  std::vector<std::uint8_t> order(afh::kChannelCount);
  for (unsigned c = 0; c < afh::kChannelCount; ++c) order[c] = static_cast<std::uint8_t>(c);
  auto rate = [&](unsigned c) { return ch_seen_b[c] ? static_cast<double>(ch_lost_b[c]) / ch_seen_b[c] : 0.0; };
  std::stable_sort(order.begin(), order.end(), [&](std::uint8_t a, std::uint8_t b) { return rate(a) > rate(b); });
  std::array<bool, afh::kChannelCount> known_bad{}, cleaned{};
  for (unsigned k = 0; k < afh::kChannelCount && k < p.known_bad_top; ++k) {
    if (ch_lost_b[order[k]] >= 2) known_bad[order[k]] = true;
  }
  const std::size_t clean_frame = p.clean_top ? (n * p.clean_at_permille) / 1000U : n;
  std::uint32_t n_cleaned = 0;
  if (p.clean_top) {
    for (unsigned k = 0; k < afh::kChannelCount && k < p.clean_top; ++k) {
      if (ch_lost_b[order[k]] >= 2) { cleaned[order[k]] = true; ++n_cleaned; }
    }
    for (std::size_t i = clean_frame; i < n; ++i) {
      if (cleaned[bch[i]]) lost[i] = 0;
    }
  }

  std::array<std::vector<std::uint32_t>, afh::kChannelCount> visits{};
  for (std::size_t i = 0; i < n; ++i) visits[bch[i]].push_back(static_cast<std::uint32_t>(i));
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
    return lost[j] == 0;
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

  std::uint64_t lost_sim = 0, frames = 0, probes = 0, probe_ok = 0, map_props = 0, activations = 0;
  std::uint64_t exclusions = 0, reinclusions = 0, reexclusions = 0, isolated_exclusions = 0,
                single_loss_exclusions = 0, fresh_single_loss_exclusions = 0, at_floor_frames = 0;
  std::uint64_t lost_sim_first_half = 0, lost_sim_second_half = 0, lost_b_first_half = 0, lost_b_second_half = 0;
  std::array<std::uint64_t, 6> by_reason{};
  std::uint64_t nb_excl = 0, nb_direct8 = 0, nb_direct4 = 0, nb_direct32_only1 = 0, nb_no_direct = 0;
  std::uint64_t post_clean_excl_of_cleaned = 0;
  std::uint8_t min_active = 40;
  std::uint8_t max_strikes = 0;
  std::vector<std::uint8_t> active_after;
  std::vector<std::uint32_t> activation_frame;
  // Last 32 data outcomes (bit = loss) SINCE the channel's last (re)inclusion: cleared at
  // re-inclusion, so losses from before (or during) a quarantine never count as evidence.
  std::array<std::uint32_t, afh::kChannelCount> recent{};
  std::array<std::uint32_t, afh::kChannelCount> ch_excl{};
  std::array<bool, afh::kChannelCount> ever_reincluded{};
  std::array<std::uint32_t, afh::kChannelCount> losses_since_incl{};  // data losses since (re)inclusion
  std::uint64_t zero_loss_reexclusions = 0;
  std::array<std::uint64_t, 6> zero_loss_by_reason{};  // exclusions with 0 losses since (re)inclusion
  std::array<std::uint64_t, 6> one_loss_by_reason{};   // ... with exactly 1
  std::vector<std::uint32_t> act_bins, bin_frames;
  std::array<long long, afh::kChannelCount> first_excl_frame{}, recover_frame{};
  first_excl_frame.fill(-1);
  recover_frame.fill(-1);
  std::array<bool, afh::kChannelCount> out_at_clean{};

  const bool trace = std::getenv("C3_TRACE") != nullptr;  // event trace on stderr
  if (trace) {
    for (unsigned c = 0; c < afh::kChannelCount; ++c) {
      std::fprintf(stderr, "B ch=%u lost=%u seen=%u%s\n", c, ch_lost_b[c], ch_seen_b[c], known_bad[c] ? " known_bad" : "");
    }
  }
  const std::uint64_t L0 = s0.L0;
  for (std::size_t i = 0; i < n; ++i) {
    const std::uint64_t f = L0 + i;
    const std::uint32_t now_ms = s0.t0_ms + static_cast<std::uint32_t>((i * period_us) / 1000U);
    if (sched.pending().valid && f >= sched.pending().activation_sequence) {
      sched.applyPendingIfDue(f);
      ++activations;
      const std::uint8_t a = sched.current().map.activeCount();
      active_after.push_back(a);
      activation_frame.push_back(static_cast<std::uint32_t>(i));
      const std::size_t b = static_cast<std::size_t>((static_cast<std::uint64_t>(i) * period_us / 1000U) / p.bin_ms);
      if (act_bins.size() <= b) act_bins.resize(b + 1, 0);
      ++act_bins[b];
      if (a < min_active) min_active = a;
    }
    if (p.clean_top && i == clean_frame) {
      for (unsigned c = 0; c < afh::kChannelCount; ++c) {
        out_at_clean[c] = cleaned[c] && !sched.current().map.isActive(static_cast<std::uint8_t>(c));
      }
    }
    if (p.clean_top && i >= clean_frame) {
      for (unsigned c = 0; c < afh::kChannelCount; ++c) {
        if (out_at_clean[c] && recover_frame[c] < 0 && sched.current().map.isActive(static_cast<std::uint8_t>(c))) {
          recover_frame[c] = static_cast<long long>(i);
        }
      }
    }
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
    const bool is_probe = probe_valid && f == probe_at;
    const std::uint8_t ch = is_probe ? probe_ch : sched.channelForSequence(f);
    const bool ok = outcomeOn(ch, i);
    ++frames;
    if (!ok) ++lost_sim;
    if (!ok) (i < n / 2 ? lost_sim_first_half : lost_sim_second_half)++;
    if (lost[i]) (i < n / 2 ? lost_b_first_half : lost_b_second_half)++;
    if (est.activeCount() <= p.q.minimum_active_channels) ++at_floor_frames;
    if (is_probe) {
      ++probes;
      if (ok) ++probe_ok;
      const bool was = est.channel(ch).state == quality::ChannelState::Probe;
      est.observeProbe(ch, ok, now_ms);
      if (trace) std::fprintf(stderr, "%u probe ch=%u ok=%d -> %d strikes=%u\n", now_ms, ch, ok ? 1 : 0,
                              static_cast<int>(est.channel(ch).state), est.channel(ch).strikes);
      if (was && est.channel(ch).state == quality::ChannelState::Active) {
        ++reinclusions;
        ever_reincluded[ch] = true;
        losses_since_incl[ch] = 0;
        recent[ch] = 0;
      }
      probe_valid = false;
    } else {
      const bool in_map_before = est.channel(ch).state == quality::ChannelState::Active ||
                                 est.channel(ch).state == quality::ChannelState::Suspect;
      if (in_map_before) {
        recent[ch] = (recent[ch] << 1U) | (ok ? 0U : 1U);
        if (!ok) ++losses_since_incl[ch];
      }
      const auto before = est.channel(ch).state;
      const std::uint8_t strikes_before = est.channel(ch).strikes;
      est.observeData(ch, ok, now_ms);
      if (before != quality::ChannelState::Excluded && est.channel(ch).state == quality::ChannelState::Excluded) {
        ++exclusions;
        ++ch_excl[ch];
        if (trace) std::fprintf(stderr, "%u EXCL ch=%u reason=%d strikes=%u active=%u\n", now_ms, ch,
                                static_cast<int>(est.channel(ch).last_exclusion_reason), est.channel(ch).strikes,
                                est.activeCount());
        if (ever_reincluded[ch]) {
          ++reexclusions;
          if (losses_since_incl[ch] == 0) ++zero_loss_reexclusions;
        }
        {
          const auto r = static_cast<std::size_t>(est.channel(ch).last_exclusion_reason);
          if (losses_since_incl[ch] == 0) ++zero_loss_by_reason[r];
          if (losses_since_incl[ch] == 1) ++one_loss_by_reason[r];
        }
        if (first_excl_frame[ch] < 0) first_excl_frame[ch] = static_cast<long long>(i);
        if (cleaned[ch] && i >= clean_frame) ++post_clean_excl_of_cleaned;
        const auto reason = est.channel(ch).last_exclusion_reason;
        ++by_reason[static_cast<std::size_t>(reason)];
        const int l8 = __builtin_popcount(recent[ch] & 0xFFU);
        const int l4 = __builtin_popcount(recent[ch] & 0x0FU);
        const int l32 = __builtin_popcount(recent[ch]);
        if (l8 <= 1) ++isolated_exclusions;
        if (l32 <= 1) {
          ++single_loss_exclusions;
          if (strikes_before == 0) ++fresh_single_loss_exclusions;
        }
        if (reason == quality::ExclusionReason::Neighbor) {
          ++nb_excl;
          if (l8 >= 1) ++nb_direct8;
          if (l4 >= 1) ++nb_direct4;
          if (l32 <= 1) ++nb_direct32_only1;
          if (l32 == 0) ++nb_no_direct;
        }
      }
      if (est.channel(ch).strikes > max_strikes) max_strikes = est.channel(ch).strikes;
    }
    if (probe_valid && f > probe_at) probe_valid = false;
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

  std::uint64_t reversals = 0;
  for (std::size_t k = 2; k < active_after.size(); ++k) {
    const int d1 = static_cast<int>(active_after[k - 1]) - static_cast<int>(active_after[k - 2]);
    const int d2 = static_cast<int>(active_after[k]) - static_cast<int>(active_after[k - 1]);
    if ((d1 > 0 && d2 < 0) || (d1 < 0 && d2 > 0)) ++reversals;
  }
  std::array<std::uint32_t, 6> act_sixth{};
  for (auto af : activation_frame) ++act_sixth[std::min<std::size_t>(5, (static_cast<std::size_t>(af) * 6) / n)];
  std::uint64_t act_first3 = 0, act_last3 = 0;
  for (auto af : activation_frame) {
    if (af < n / 3) ++act_first3;
    else if (af >= n - n / 3) ++act_last3;
  }
  std::uint64_t lost_b = 0;
  for (std::size_t i = 0; i < n; ++i) lost_b += lost[i];
  // Same computation as c2_replay (std::sort of (rate, exclusions) pairs) for comparability.
  std::vector<std::pair<double, std::uint32_t>> rank;
  for (unsigned c = 0; c < afh::kChannelCount; ++c) rank.emplace_back(rate(c), ch_excl[c]);
  std::sort(rank.begin(), rank.end(), [](auto& a, auto& b) { return a.first > b.first; });
  std::uint32_t excl_worst8 = 0, excl_total = 0;
  for (std::size_t k = 0; k < rank.size(); ++k) {
    excl_total += rank[k].second;
    if (k < 8) excl_worst8 += rank[k].second;
  }
  const double ms_per_frame = period_us / 1000.0;
  {
    const std::size_t nb = static_cast<std::size_t>((static_cast<std::uint64_t>(n - 1) * period_us / 1000U) / p.bin_ms) + 1;
    act_bins.resize(nb, 0);
    bin_frames.assign(nb, 0);
    const std::uint64_t frames_per_bin = static_cast<std::uint64_t>(p.bin_ms) * 1000U / period_us;
    for (std::size_t b = 0; b < nb; ++b) {
      const std::uint64_t lo = b * frames_per_bin;
      const std::uint64_t hi = std::min<std::uint64_t>(n, (b + 1) * frames_per_bin);
      bin_frames[b] = static_cast<std::uint32_t>(hi > lo ? hi - lo : 0);
    }
  }
  std::vector<double> act_bins_d(act_bins.begin(), act_bins.end()), bin_frames_d(bin_frames.begin(), bin_frames.end());
  std::vector<double> detect_ms;
  std::uint32_t known_bad_n = 0, known_bad_detected = 0;
  double detect_sum_censored = 0;
  for (unsigned c = 0; c < afh::kChannelCount; ++c) {
    if (!known_bad[c]) continue;
    ++known_bad_n;
    if (first_excl_frame[c] >= 0) {
      ++known_bad_detected;
      detect_ms.push_back(first_excl_frame[c] * ms_per_frame);
      detect_sum_censored += first_excl_frame[c] * ms_per_frame;
    } else {
      detect_sum_censored += n * ms_per_frame;
    }
  }
  std::vector<double> recover_ms;
  std::uint32_t out_at_clean_n = 0, recovered = 0;
  double recover_sum_censored = 0;
  for (unsigned c = 0; c < afh::kChannelCount; ++c) {
    if (!out_at_clean[c]) continue;
    ++out_at_clean_n;
    if (recover_frame[c] >= 0) {
      ++recovered;
      const double t = (recover_frame[c] - static_cast<long long>(clean_frame)) * ms_per_frame;
      recover_ms.push_back(t);
      recover_sum_censored += t;
    } else {
      recover_sum_censored += (n - clean_frame) * ms_per_frame;
    }
  }
  const double dur_s = static_cast<double>(n) * period_us / 1e6;
  std::printf(
      "{\"run\":\"%s\",\"frames\":%llu,\"lost_b\":%llu,\"lost_sim\":%llu,\"loss_b_pct\":%.4f,\"loss_sim_pct\":%.4f,"
      "\"map_proposals\":%llu,\"activations\":%llu,\"activations_per_min\":%.2f,\"reversals\":%llu,"
      "\"exclusions\":%llu,\"isolated_exclusions\":%llu,\"single_loss_exclusions\":%llu,\"reinclusions\":%llu,\"probes\":%llu,\"probe_ok\":%llu,"
      "\"min_active\":%u,\"final_active\":%u,\"floor_frac\":%.4f,\"excl_on_worst8\":%u,\"excl_total\":%u,"
      "\"duration_s\":%.1f,"
      "\"reexclusions\":%llu,\"fresh_single_loss_exclusions\":%llu,\"max_strikes\":%u,"
      "\"excl_by_reason\":{\"consec\":%llu,\"fast\":%llu,\"slow\":%llu,\"neighbor\":%llu,\"probation\":%llu},"
      "\"nb_excl\":%llu,\"nb_direct8\":%llu,\"nb_direct4\":%llu,\"nb_only1_in32\":%llu,\"nb_no_direct\":%llu,"
      "\"act_first_third\":%llu,\"act_sixths\":[%u,%u,%u,%u,%u,%u],\"zero_loss_reexclusions\":%llu,\"act_last_third\":%llu,"
      "\"known_bad\":%u,\"known_bad_detected\":%u,\"detect_ms\":%s,\"detect_mean_censored_ms\":%.0f,"
      "\"lost_sim_h1\":%llu,\"lost_sim_h2\":%llu,\"lost_b_h1\":%llu,\"lost_b_h2\":%llu,"
      "\"cleaned\":%u,\"out_at_clean\":%u,\"recovered\":%u,\"recover_ms\":%s,\"recover_mean_censored_ms\":%.0f,"
      "\"post_clean_excl_of_cleaned\":%llu,\"recover_window_ms\":%.0f,"
      "\"zero_loss_by_reason\":{\"consec\":%llu,\"fast\":%llu,\"slow\":%llu,\"neighbor\":%llu,\"probation\":%llu},"
      "\"one_loss_by_reason\":{\"consec\":%llu,\"fast\":%llu,\"slow\":%llu,\"neighbor\":%llu,\"probation\":%llu},"
      "\"bin_ms\":%u,\"act_bins\":%s,\"bin_frames\":%s,\"period_us\":%u}\n",
      run.c_str(), static_cast<unsigned long long>(frames), static_cast<unsigned long long>(lost_b),
      static_cast<unsigned long long>(lost_sim), 100.0 * lost_b / n, 100.0 * lost_sim / frames,
      static_cast<unsigned long long>(map_props), static_cast<unsigned long long>(activations),
      activations * 60.0 / dur_s, static_cast<unsigned long long>(reversals),
      static_cast<unsigned long long>(exclusions), static_cast<unsigned long long>(isolated_exclusions),
      static_cast<unsigned long long>(single_loss_exclusions),
      static_cast<unsigned long long>(reinclusions), static_cast<unsigned long long>(probes),
      static_cast<unsigned long long>(probe_ok), static_cast<unsigned>(min_active),
      static_cast<unsigned>(sched.current().map.activeCount()), static_cast<double>(at_floor_frames) / frames,
      excl_worst8, excl_total, dur_s,
      static_cast<unsigned long long>(reexclusions), static_cast<unsigned long long>(fresh_single_loss_exclusions),
      static_cast<unsigned>(max_strikes),
      static_cast<unsigned long long>(by_reason[1]), static_cast<unsigned long long>(by_reason[2]),
      static_cast<unsigned long long>(by_reason[3]), static_cast<unsigned long long>(by_reason[4]),
      static_cast<unsigned long long>(by_reason[5]),
      static_cast<unsigned long long>(nb_excl), static_cast<unsigned long long>(nb_direct8),
      static_cast<unsigned long long>(nb_direct4), static_cast<unsigned long long>(nb_direct32_only1),
      static_cast<unsigned long long>(nb_no_direct),
      static_cast<unsigned long long>(act_first3), act_sixth[0], act_sixth[1], act_sixth[2], act_sixth[3], act_sixth[4],
      act_sixth[5], static_cast<unsigned long long>(zero_loss_reexclusions), static_cast<unsigned long long>(act_last3),
      known_bad_n, known_bad_detected, jsonList(detect_ms).c_str(),
      known_bad_n ? detect_sum_censored / known_bad_n : -1.0,
      static_cast<unsigned long long>(lost_sim_first_half), static_cast<unsigned long long>(lost_sim_second_half),
      static_cast<unsigned long long>(lost_b_first_half), static_cast<unsigned long long>(lost_b_second_half),
      n_cleaned, out_at_clean_n, recovered, jsonList(recover_ms).c_str(),
      out_at_clean_n ? recover_sum_censored / out_at_clean_n : -1.0,
      static_cast<unsigned long long>(post_clean_excl_of_cleaned), (n - clean_frame) * ms_per_frame,
      static_cast<unsigned long long>(zero_loss_by_reason[1]), static_cast<unsigned long long>(zero_loss_by_reason[2]),
      static_cast<unsigned long long>(zero_loss_by_reason[3]), static_cast<unsigned long long>(zero_loss_by_reason[4]),
      static_cast<unsigned long long>(zero_loss_by_reason[5]),
      static_cast<unsigned long long>(one_loss_by_reason[1]), static_cast<unsigned long long>(one_loss_by_reason[2]),
      static_cast<unsigned long long>(one_loss_by_reason[3]), static_cast<unsigned long long>(one_loss_by_reason[4]),
      static_cast<unsigned long long>(one_loss_by_reason[5]),
      p.bin_ms, jsonList(act_bins_d).c_str(), jsonList(bin_frames_d).c_str(), period_us);
  return 0;
}
