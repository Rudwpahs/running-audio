#pragma once

// Gate C (issue #52): adaptive channel map on top of the Gate B static AFH.
// The RX owns channel-quality decisions (pr1_channel_quality.hpp). Map changes and
// probes are coordinated through an out-of-band bench control plane (USB/host
// relay) and always take effect at the same future logical frame on both sides.
// The wireless reverse link is intentionally NOT implemented in this gate.

#include <array>
#include <cstdint>

#include "../../common/pr1_afh.hpp"
#include "../../common/pr1_channel_quality.hpp"
#include "../../common/pr1_instrumentation.hpp"
#include "../../common/pr1_placement.hpp"

#ifndef PR1_ENABLE_ADAPTIVE_MAP
#define PR1_ENABLE_ADAPTIVE_MAP 0
#endif

// Frames between a proposal and its activation (~1.8 s at the 150 us gap). Must be
// far longer than the host relay round trip (a few ms).
#ifndef PR1_MAP_LEAD_FRAMES
#define PR1_MAP_LEAD_FRAMES 600
#endif

// A commit is refused if fewer frames than this remain before activation.
#ifndef PR1_MAP_GUARD_FRAMES
#define PR1_MAP_GUARD_FRAMES 100
#endif

// Gate C2: minimum time between two map proposals (0 = Gate C/C1: no cap).
#ifndef PR1_MAP_MIN_INTERVAL_MS
#define PR1_MAP_MIN_INTERVAL_MS 0
#endif

namespace pr1::runtime::amap {

constexpr bool kEnabled = PR1_ENABLE_ADAPTIVE_MAP != 0;

// Estimator configuration. Unset macros keep the pr1_channel_quality.hpp defaults (Gate C/C1);
// Gate C2 images set the values chosen by the offline replay (analysis/c2_sweep.py).
inline quality::Config qualityConfig() {
  quality::Config c{};
#ifdef PR1_Q_EXCLUDE_PDR_Q15
  c.exclude_pdr_q15 = PR1_Q_EXCLUDE_PDR_Q15;
#endif
#ifdef PR1_Q_EXCLUDE_SLOW_PDR_Q15
  c.exclude_slow_pdr_q15 = PR1_Q_EXCLUDE_SLOW_PDR_Q15;
#endif
#ifdef PR1_Q_REINSTATE_SUCCESSES
  c.reinstate_probe_successes = PR1_Q_REINSTATE_SUCCESSES;
#endif
#ifdef PR1_Q_PROBE_INITIAL_MS
  c.initial_probe_ms = PR1_Q_PROBE_INITIAL_MS;
#endif
#ifdef PR1_Q_PROBE_MAX_MS
  c.max_probe_ms = PR1_Q_PROBE_MAX_MS;
#endif
  // Gate C3 knobs (all default-off in pr1_channel_quality.hpp).
#ifdef PR1_Q_STRIKE_BACKOFF_MAX_SHIFT
  c.strike_backoff_max_shift = PR1_Q_STRIKE_BACKOFF_MAX_SHIFT;
#endif
#ifdef PR1_Q_STRIKE_PROBE_MS
  c.strike_probe_ms = PR1_Q_STRIKE_PROBE_MS;
#endif
#ifdef PR1_Q_STRIKE_MAX_PROBE_MS
  c.strike_max_probe_ms = PR1_Q_STRIKE_MAX_PROBE_MS;
#endif
#ifdef PR1_Q_STRIKE_DECAY_MS
  c.strike_decay_ms = PR1_Q_STRIKE_DECAY_MS;
#endif
#ifdef PR1_Q_NEIGHBOR_RADIUS
  c.neighbor_radius = PR1_Q_NEIGHBOR_RADIUS;
#endif
#ifdef PR1_Q_NEIGHBOR_MIN_BAD
  c.neighbor_min_bad = PR1_Q_NEIGHBOR_MIN_BAD;
#endif
#ifdef PR1_Q_NEIGHBOR_BAD_SLOW_Q15
  c.neighbor_bad_slow_q15 = PR1_Q_NEIGHBOR_BAD_SLOW_Q15;
#endif
#ifdef PR1_Q_NEIGHBOR_DIRECT_FAST_Q15
  c.neighbor_direct_fast_q15 = PR1_Q_NEIGHBOR_DIRECT_FAST_Q15;
#endif
  return c;
}

enum class OutcomeKind : std::uint8_t { Ok = 0, Crc = 1, Timeout = 2 };

// Queued from the RX hot path (no computation there), consumed in the idle loop.
struct Outcome {
  std::uint32_t logical_lo = 0;
  std::uint32_t t_ms = 0;
  std::uint8_t channel = 0;
  OutcomeKind kind = OutcomeKind::Ok;
  bool probe = false;
};

enum class ProposalType : std::uint8_t { None = 0, Map = 1, Probe = 2 };

struct Proposal {
  ProposalType type = ProposalType::None;
  std::uint16_t id = 0;
  std::uint16_t version = 0;      // map: new version; probe: map version at proposal
  std::uint64_t bits = 0;         // map: proposed bitmap
  std::uint64_t activation = 0;   // map: activation frame; probe: probe frame
  std::uint8_t channel = 0;       // probe channel
};

struct ProbeSlot {
  bool valid = false;
  std::uint16_t id = 0;
  std::uint64_t at = 0;
  std::uint8_t channel = 0;
};

enum class EventKind : std::uint8_t {
  Proposed = 0,
  Committed = 1,
  CommitLate = 2,
  Expired = 3,
  Activated = 4,
  TxStaged = 5,
  TxRejected = 6,
  Suspect = 7,
  Excluded = 8,
  ProbeResult = 9,
  Reincluded = 10,
  Recovered = 11,  // SUSPECT -> ACTIVE without exclusion
};

struct Event {
  std::uint32_t t_ms = 0;
  std::uint32_t logical_lo = 0;   // RX/TX logical frame when the event happened
  std::uint32_t target_lo = 0;    // activation / probe frame
  std::uint64_t bits = 0;
  std::uint16_t id = 0;
  std::uint16_t version = 0;
  std::uint8_t channel = 0;
  std::uint8_t value = 0;         // probe: 1 ok / 0 fail; exclusion: 1 losses / 2 pdr; active count
  EventKind kind = EventKind::Proposed;
};

struct ChannelCounters {
  std::uint32_t ok = 0;
  std::uint32_t crc = 0;
  std::uint32_t timeout = 0;
  std::uint32_t probe_ok = 0;
  std::uint32_t probe_fail = 0;
  std::uint16_t suspects = 0;
  std::uint16_t exclusions = 0;
  std::uint16_t reinclusions = 0;
  std::uint32_t last_excluded_ms = 0;
};

struct Telemetry {
  static constexpr std::size_t kEvents = 160;
  std::array<Event, kEvents> events{};
  std::uint32_t events_written = 0;
  std::array<ChannelCounters, afh::kChannelCount> channels{};
  std::uint32_t outcomes_dropped = 0;
  std::uint32_t proposals = 0;
  std::uint32_t commits = 0;
  std::uint32_t commit_late = 0;
  std::uint32_t expired = 0;
  std::uint32_t activations = 0;
  std::uint32_t tx_rejects = 0;
  std::uint8_t min_active_seen = afh::kChannelCount;
  std::uint32_t emit_dropped = 0;
  instrumentation::DurationWindow<256> ctrl_us{};  // control-record handling time (radio core)

  // Separate small activation log so TX/RX agreement can be checked even if the
  // (RX-heavy) event list overflows.
  struct Activation {
    std::uint16_t version = 0;
    std::uint32_t activation_lo = 0;
    std::uint32_t applied_at_lo = 0;
  };
  static constexpr std::size_t kActivations = 32;
  std::array<Activation, kActivations> activation_log{};
  std::uint32_t activation_count = 0;
  PR1_IRAM void logActivation(std::uint16_t version, std::uint64_t activation, std::uint64_t applied_at) {
    if (activation_count < kActivations) {
      activation_log[activation_count] = {version, static_cast<std::uint32_t>(activation),
                                          static_cast<std::uint32_t>(applied_at)};
    }
    ++activation_count;
  }

  PR1_IRAM void record(const Event& e) {
    if (events_written < kEvents) events[events_written] = e;
    ++events_written;
  }
};

template <std::size_t N>
struct OutcomeQueue {
  std::array<Outcome, N> items{};
  std::uint32_t head = 0;
  std::uint32_t tail = 0;
  PR1_IRAM bool push(const Outcome& o) {
    if (head - tail >= N) return false;
    items[head % N] = o;
    ++head;
    return true;
  }
  PR1_IRAM bool pop(Outcome* out) {
    if (tail == head) return false;
    *out = items[tail % N];
    ++tail;
    return true;
  }
};

}  // namespace pr1::runtime::amap
