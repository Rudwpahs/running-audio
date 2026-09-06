#pragma once

namespace pr1::board {

struct Sx1280Pins {
  int cs;
  int rst;
  int sclk;
  int mosi;
  int miso;
  int dio1;
  int busy;
  int tx_enable;
  int rx_enable;
};

// Upstream hardware reference only. These values must be checked against the
// exact physical T3-S3-MVSRBoard revision before any RF-enabled build is used.
inline constexpr char kBoardFamily[] = "LILYGO T3-S3-MVSRBoard";
inline constexpr char kReferenceRevision[] = "V1.1 upstream reference";
inline constexpr char kRadioTarget[] = "SX1280";
inline constexpr char kUpstreamReferenceCommit[] =
    "840a2e788b3192c4e9bddf0640c1ecaf703c2598";

inline constexpr Sx1280Pins kSx1280Pins{
    7,   // CS
    8,   // RST
    5,   // SCLK
    6,   // MOSI
    3,   // MISO
    9,   // DIO1
    36,  // BUSY
    10,  // TX RF-switch control
    21,  // RX RF-switch control
};

}  // namespace pr1::board
