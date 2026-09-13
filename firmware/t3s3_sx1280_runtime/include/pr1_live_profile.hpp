#pragma once

#include <cstdint>

#ifndef PR1_RF_ENABLED
#define PR1_RF_ENABLED 0
#endif

#ifndef PR1_RUNTIME_ROLE
#define PR1_RUNTIME_ROLE 0
#endif

#ifndef PR1_TX_GAP_US
#define PR1_TX_GAP_US 5000
#endif

#define PR1_RUNTIME_ROLE_SAFE 0
#define PR1_RUNTIME_ROLE_TX 1
#define PR1_RUNTIME_ROLE_RX 2

#if (PR1_RF_ENABLED != 0) && (PR1_RF_ENABLED != 1)
#error "PR1_RF_ENABLED must be 0 or 1"
#endif

#if (PR1_RUNTIME_ROLE < PR1_RUNTIME_ROLE_SAFE) || (PR1_RUNTIME_ROLE > PR1_RUNTIME_ROLE_RX)
#error "PR1_RUNTIME_ROLE must be SAFE(0), TX(1), or RX(2)"
#endif

#if (PR1_RF_ENABLED == 0) && (PR1_RUNTIME_ROLE != PR1_RUNTIME_ROLE_SAFE)
#error "RF-disabled builds must use the SAFE runtime role"
#endif

#if (PR1_RF_ENABLED == 1) && (PR1_RUNTIME_ROLE == PR1_RUNTIME_ROLE_SAFE)
#error "RF-enabled builds must explicitly select TX or RX runtime role"
#endif

#if PR1_TX_GAP_US < 0
#error "PR1_TX_GAP_US must be >= 0"
#endif

namespace pr1::runtime {

enum class RuntimeRole : std::uint8_t {
  Safe = PR1_RUNTIME_ROLE_SAFE,
  Tx = PR1_RUNTIME_ROLE_TX,
  Rx = PR1_RUNTIME_ROLE_RX,
};

struct FixedFlrcProfile {
  float frequency_mhz;
  std::uint16_t bitrate_kbps;
  std::uint8_t coding_rate;
  std::int8_t output_dbm;
  // Intentional historical semantics: idle time after a blocking TX completes,
  // not packet start-to-start period. This makes the V4 500..0 us sweep reproducible.
  std::uint32_t tx_gap_us;
};

inline constexpr FixedFlrcProfile kFixedFlrcProfile{
    2404.0F,
    1300U,
    3U,
    0,
    static_cast<std::uint32_t>(PR1_TX_GAP_US),
};

constexpr RuntimeRole runtimeRole() {
#if PR1_RUNTIME_ROLE == PR1_RUNTIME_ROLE_TX
  return RuntimeRole::Tx;
#elif PR1_RUNTIME_ROLE == PR1_RUNTIME_ROLE_RX
  return RuntimeRole::Rx;
#else
  return RuntimeRole::Safe;
#endif
}

constexpr bool liveRfEnabled() {
  return PR1_RF_ENABLED == 1 && runtimeRole() != RuntimeRole::Safe;
}

constexpr const char* runtimeRoleName() {
  switch (runtimeRole()) {
    case RuntimeRole::Tx:
      return "tx";
    case RuntimeRole::Rx:
      return "rx";
    case RuntimeRole::Safe:
    default:
      return "safe";
  }
}

constexpr const char* runtimeProfileName() {
  switch (runtimeRole()) {
    case RuntimeRole::Tx:
      return "phase1-fixed-flrc-tx";
    case RuntimeRole::Rx:
      return "phase1-fixed-flrc-rx";
    case RuntimeRole::Safe:
    default:
      return "round2-safe";
  }
}

}  // namespace pr1::runtime
