#pragma once

#include <cstddef>
#include <cstdint>

#include "../../common/pr1_packet.hpp"
#include "pr1_board_config.hpp"

static_assert(__cplusplus >= 201703L,
              "PR1 T3-S3/SX1280 runtime requires C++17 or newer");

#ifndef PR1_RF_ENABLED
#define PR1_RF_ENABLED 0
#endif

#if (PR1_RF_ENABLED != 0) && (PR1_RF_ENABLED != 1)
#error "PR1_RF_ENABLED must be 0 or 1"
#endif

namespace pr1::runtime {

struct BootMetadata {
  const char* runtime_profile;
  const char* board_family;
  const char* board_reference_revision;
  const char* radio_target;
  const char* upstream_reference_commit;
  bool hardware_verified;
  bool rf_enabled;
  std::uint8_t protocol_version;
  std::size_t protocol_header_bytes;
  board::Sx1280Pins pins;
};

inline constexpr BootMetadata kBootMetadata{
    "round2-safe",
    board::kBoardFamily,
    board::kReferenceRevision,
    board::kRadioTarget,
    board::kUpstreamReferenceCommit,
    false,
    PR1_RF_ENABLED != 0,
    pr1::kVersion,
    pr1::kHeaderBytes,
    board::kSx1280Pins,
};

static_assert(pr1::kRadioPayloadMaxBytes == 127,
              "Runtime must preserve the SX1280 FLRC 127-byte payload ceiling");
static_assert(pr1::kDartPacketBytes == 116,
              "Runtime must preserve the PR1-DART 116-byte baseline packet");

}  // namespace pr1::runtime
