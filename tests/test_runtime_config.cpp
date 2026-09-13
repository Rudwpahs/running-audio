#include <cassert>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <string_view>

#include "../firmware/t3s3_sx1280_runtime/include/pr1_live_profile.hpp"
#include "../firmware/t3s3_sx1280_runtime/include/pr1_runtime_config.hpp"
#include "../firmware/t3s3_sx1280_runtime/include/pr1_safe_telemetry.hpp"

int main() {
  using namespace pr1::runtime;

  static_assert(!kBootMetadata.rf_enabled, "Safe build must default to RF disabled");
  static_assert(!kBootMetadata.hardware_verified,
                "CI must not claim physical hardware verification");
  static_assert(kBootMetadata.protocol_version == 1, "Unexpected PR1 protocol version");
  static_assert(kBootMetadata.protocol_header_bytes == 16,
                "Unexpected PR1 protocol header length");

  // Phase-1 live-runtime contract: safe is still the default, and the
  // first explicit RF profile is a fixed FLRC baseline with all adaptive
  // layers kept out of this runtime profile.
  static_assert(runtimeRole() == RuntimeRole::Safe,
                "Default host/safe build must resolve to RuntimeRole::Safe");
  static_assert(!liveRfEnabled(), "Default host/safe build must not enable RF");
  static_assert(kFixedFlrcProfile.bitrate_kbps == 1300u,
                "Initial fixed FLRC baseline must be 1.3 Mbps");
  static_assert(kFixedFlrcProfile.coding_rate == 3u,
                "Initial fixed FLRC baseline must use CR 3/4");
  static_assert(pr1::kDartPacketBytes == 116u);
  static_assert(pr1::kDartPacketBytes <= pr1::kRadioPayloadMaxBytes,
                "Canonical PR1-DART packet must fit the SX1280 FLRC payload ceiling");

  static_assert(kBootMetadata.pins.cs == 7);
  static_assert(kBootMetadata.pins.rst == 8);
  static_assert(kBootMetadata.pins.sclk == 5);
  static_assert(kBootMetadata.pins.mosi == 6);
  static_assert(kBootMetadata.pins.miso == 3);
  static_assert(kBootMetadata.pins.dio1 == 9);
  static_assert(kBootMetadata.pins.busy == 36);
  static_assert(kBootMetadata.pins.tx_enable == 10);
  static_assert(kBootMetadata.pins.rx_enable == 21);

  static_assert(pr1::telemetry::kTelemetrySchemaVersion == 1);
  static_assert(pr1::telemetry::capabilityMask(pr1::telemetry::Capability::TimingTrace) == 8u);

  constexpr auto snapshot = makeSafeTelemetrySnapshot();
  static_assert(snapshot.state == pr1::telemetry::DeviceState::SafeIdle);
  static_assert(snapshot.capability_mask == 8u);
  static_assert(!snapshot.rssi_dbm.available);
  static_assert(!snapshot.queue_depth.available);
  static_assert(!snapshot.irq_to_spi_us.available);
  static_assert(!snapshot.rx_processing_us.available);
  static_assert(!snapshot.rx_rearm_us.available);

  bool saw_max_queue_depth = false;
  bool saw_arq_retransmit = false;
  pr1::telemetry::forEachSnapshotField(snapshot, [&](pr1::telemetry::FieldValue item) {
    if (item.field == pr1::telemetry::FieldId::MaxQueueDepth) saw_max_queue_depth = true;
    if (item.field == pr1::telemetry::FieldId::ArqRetransmitSent) saw_arq_retransmit = true;
  });
  assert(!saw_max_queue_depth);
  assert(!saw_arq_retransmit);

  assert(std::string_view{kBootMetadata.board_family} == "LILYGO T3-S3-MVSRBoard");
  assert(std::string_view{kBootMetadata.radio_target} == "SX1280");
  std::cout << "test_runtime_config: PASS\n";
  return 0;
}
