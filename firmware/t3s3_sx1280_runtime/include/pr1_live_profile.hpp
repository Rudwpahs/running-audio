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

// Link-budget tuning overrides. Defaults are the frozen fixed-FLRC baseline.
#ifndef PR1_FREQ_KHZ
#define PR1_FREQ_KHZ 2404000
#endif

#ifndef PR1_FLRC_BITRATE_KBPS
#define PR1_FLRC_BITRATE_KBPS 1300
#endif

#ifndef PR1_FLRC_CR
#define PR1_FLRC_CR 3
#endif

#ifndef PR1_TX_OUTPUT_DBM
#define PR1_TX_OUTPUT_DBM 0
#endif

#if (PR1_FREQ_KHZ < 2400000) || (PR1_FREQ_KHZ > 2483500)
#error "PR1_FREQ_KHZ must be inside the 2400..2483.5 MHz ISM band"
#endif

#if (PR1_FLRC_BITRATE_KBPS != 1300) && (PR1_FLRC_BITRATE_KBPS != 1000) && \
    (PR1_FLRC_BITRATE_KBPS != 650) && (PR1_FLRC_BITRATE_KBPS != 520) &&   \
    (PR1_FLRC_BITRATE_KBPS != 325) && (PR1_FLRC_BITRATE_KBPS != 260)
#error "PR1_FLRC_BITRATE_KBPS must be a RadioLib FLRC bitrate"
#endif

// RadioLib 7.7.1 FLRC coding rate argument: 2 = 1/2, 3 = 3/4, 4 = 1/1.
#if (PR1_FLRC_CR < 2) || (PR1_FLRC_CR > 4)
#error "PR1_FLRC_CR must be 2 (1/2), 3 (3/4) or 4 (1/1)"
#endif

// T3-S3 SX1280 PA variant: the chip drives an external PA, so the chip output
// must stay within -18..+3 dBm (LilyGo reference limit).
#if (PR1_TX_OUTPUT_DBM < -18) || (PR1_TX_OUTPUT_DBM > 3)
#error "PR1_TX_OUTPUT_DBM must be -18..3 dBm on the SX1280 PA board"
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
    static_cast<float>(PR1_FREQ_KHZ) / 1000.0F,
    static_cast<std::uint16_t>(PR1_FLRC_BITRATE_KBPS),
    static_cast<std::uint8_t>(PR1_FLRC_CR),
    static_cast<std::int8_t>(PR1_TX_OUTPUT_DBM),
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
