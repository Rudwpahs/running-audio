#include <cassert>
#include <cstdint>
#include <iostream>
#include <string_view>
#include <vector>

#include "../firmware/common/pr1_telemetry.hpp"

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

  Snapshot snapshot{};
  snapshot.state = DeviceState::SafeIdle;
  snapshot.capability_mask = capabilityMask(Capability::TimingTrace);
  snapshot.counters.crc_bad = 4;
  snapshot.counters.scheduler_misses = 2;
  snapshot.counters.max_queue_depth = 7;
  snapshot.counters.retransmit_sent = 2;
  snapshot.trace_overwrites = 3;
  snapshot.rssi_dbm = {false, -41};
  snapshot.irq_to_spi_us = {true, 177};

  std::vector<FieldValue> fields;
  forEachSnapshotField(snapshot, [&](FieldValue value) { fields.push_back(value); });

  assert(!fields.empty());
  assert(fields.front().field == FieldId::DeviceState);
  assert(fields.front().value == 1);
  assert(fields.back().field == FieldId::CapabilityMask);

  bool saw_rssi = false;
  bool saw_irq = false;
  bool saw_crc_bad = false;
  bool saw_max_queue = false;
  bool saw_arq_retransmit = false;
  for (const auto& field : fields) {
    if (field.field == FieldId::RssiDbm) saw_rssi = true;
    if (field.field == FieldId::IrqToSpiUs && field.value == 177) saw_irq = true;
    if (field.field == FieldId::CrcBad && field.value == 4) saw_crc_bad = true;
    if (field.field == FieldId::MaxQueueDepth) saw_max_queue = true;
    if (field.field == FieldId::ArqRetransmitSent) saw_arq_retransmit = true;
  }
  assert(!saw_rssi);
  assert(saw_irq);
  assert(saw_crc_bad);
  assert(!saw_max_queue);
  assert(!saw_arq_retransmit);

  Snapshot observed{};
  observed.state = DeviceState::Ready;
  observed.capability_mask = capabilityMask(Capability::TimingTrace) |
                             capabilityMask(Capability::RfStats) |
                             capabilityMask(Capability::Arq);
  observed.max_queue_depth = {true, 9};
  observed.arq_retransmit_sent = {true, 3};

  bool observed_saw_max_queue = false;
  bool observed_saw_arq_retransmit = false;
  forEachSnapshotField(observed, [&](FieldValue value) {
    if (value.field == FieldId::MaxQueueDepth && value.value == 9) {
      observed_saw_max_queue = true;
    }
    if (value.field == FieldId::ArqRetransmitSent && value.value == 3) {
      observed_saw_arq_retransmit = true;
    }
  });
  assert(observed_saw_max_queue);
  assert(observed_saw_arq_retransmit);

  std::cout << "test_telemetry: PASS\n";
  return 0;
}
