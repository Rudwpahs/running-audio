#include <cassert>
#include <cstdint>
#include <iostream>
#include <string_view>
#include <vector>

#include "../firmware/common/pr1_telemetry.hpp"
#include "../firmware/t3s3_sx1280_runtime/include/pr1_safe_telemetry.hpp"

namespace {

bool hasField(const std::vector<pr1::telemetry::FieldValue>& fields,
              pr1::telemetry::FieldId id) {
  for (const auto& field : fields) {
    if (field.field == id) return true;
  }
  return false;
}

bool hasFieldValue(const std::vector<pr1::telemetry::FieldValue>& fields,
                   pr1::telemetry::FieldId id, std::int64_t expected) {
  for (const auto& field : fields) {
    if (field.field == id && field.value == expected) return true;
  }
  return false;
}

std::vector<pr1::telemetry::FieldValue> collect(
    const pr1::telemetry::Snapshot& snapshot) {
  std::vector<pr1::telemetry::FieldValue> fields;
  pr1::telemetry::forEachSnapshotField(
      snapshot, [&](pr1::telemetry::FieldValue value) { fields.push_back(value); });
  return fields;
}

}  // namespace

int main() {
  using namespace pr1::telemetry;

  static_assert(kTelemetrySchemaVersion == 1);
  static_assert(static_cast<std::uint8_t>(FieldId::DeviceState) == 0x01);
  static_assert(static_cast<std::uint8_t>(FieldId::CapabilityMask) == 0x14);
  static_assert(static_cast<std::uint32_t>(Capability::TimingTrace) == 8u);

  assert(std::string_view{deviceStateName(DeviceState::SafeIdle)} == "safe_idle");
  assert(std::string_view{fieldName(FieldId::CrcBad)} == "crc_bad");
  assert(std::string_view{eventName(pr1::instrumentation::Event::RxPacketOk)} ==
         "rx_packet_ok");
  assert(std::string_view{eventName(pr1::instrumentation::Event::RxCrcFail)} ==
         "rx_crc_fail");
  assert(std::string_view{eventName(pr1::instrumentation::Event::RxRearmStart)} ==
         "rx_rearm_start");
  assert(std::string_view{eventName(pr1::instrumentation::Event::RxRearmDone)} ==
         "rx_rearm_done");
  assert(std::string_view{eventName(pr1::instrumentation::Event::QueueDepth)} ==
         "queue_depth");

  // Safe mode has no RF path. Zero-initialized counters must not masquerade as
  // observed zero-loss measurements.
  const Snapshot safe = pr1::runtime::makeSafeTelemetrySnapshot();
  const auto safe_fields = collect(safe);
  assert(hasFieldValue(safe_fields, FieldId::DeviceState, 1));
  assert(hasFieldValue(safe_fields, FieldId::CapabilityMask,
                       capabilityMask(Capability::TimingTrace)));
  assert(!hasField(safe_fields, FieldId::CrcGood));
  assert(!hasField(safe_fields, FieldId::CrcBad));
  assert(!hasField(safe_fields, FieldId::Missing));
  assert(!hasField(safe_fields, FieldId::SchedulerMisses));

  // Internal counters may hold values, but they are not host-visible until the
  // owning runtime explicitly marks the corresponding measurement observed.
  Snapshot unobserved{};
  unobserved.state = DeviceState::SafeIdle;
  unobserved.capability_mask = capabilityMask(Capability::TimingTrace);
  unobserved.counters.crc_good = 20;
  unobserved.counters.crc_bad = 4;
  unobserved.counters.missing = 3;
  unobserved.counters.scheduler_misses = 2;
  unobserved.counters.max_queue_depth = 7;
  unobserved.counters.retransmit_sent = 2;
  unobserved.trace_overwrites = 3;
  unobserved.rssi_dbm = {false, -41};
  unobserved.irq_to_spi_us = {true, 177};

  const auto unobserved_fields = collect(unobserved);
  assert(!hasField(unobserved_fields, FieldId::RssiDbm));
  assert(hasFieldValue(unobserved_fields, FieldId::IrqToSpiUs, 177));
  assert(!hasField(unobserved_fields, FieldId::CrcGood));
  assert(!hasField(unobserved_fields, FieldId::CrcBad));
  assert(!hasField(unobserved_fields, FieldId::Missing));
  assert(!hasField(unobserved_fields, FieldId::SchedulerMisses));
  assert(!hasField(unobserved_fields, FieldId::MaxQueueDepth));
  assert(!hasField(unobserved_fields, FieldId::ArqRetransmitSent));

  // Observed zero is a real value and must remain representable.
  Snapshot observed{};
  observed.state = DeviceState::Ready;
  observed.capability_mask = capabilityMask(Capability::TimingTrace) |
                             capabilityMask(Capability::RfStats) |
                             capabilityMask(Capability::Arq);
  observed.crc_good = {true, 0};
  observed.crc_bad = {true, 4};
  observed.missing = {true, 0};
  observed.scheduler_misses = {true, 0};
  observed.max_queue_depth = {true, 9};
  observed.arq_retransmit_sent = {true, 3};

  const auto observed_fields = collect(observed);
  assert(hasFieldValue(observed_fields, FieldId::CrcGood, 0));
  assert(hasFieldValue(observed_fields, FieldId::CrcBad, 4));
  assert(hasFieldValue(observed_fields, FieldId::Missing, 0));
  assert(hasFieldValue(observed_fields, FieldId::SchedulerMisses, 0));
  assert(hasFieldValue(observed_fields, FieldId::MaxQueueDepth, 9));
  assert(hasFieldValue(observed_fields, FieldId::ArqRetransmitSent, 3));

  std::cout << "test_telemetry: PASS\n";
  return 0;
}
