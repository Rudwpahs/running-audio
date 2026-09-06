#pragma once

#include <cstdint>

#include "pr1_instrumentation.hpp"

namespace pr1::telemetry {

inline constexpr std::uint8_t kTelemetrySchemaVersion = 1;

enum class DeviceState : std::uint8_t {
  Booting = 0,
  SafeIdle = 1,
  Ready = 2,
  Streaming = 3,
  Degraded = 4,
  Fault = 5,
};

enum class Capability : std::uint32_t {
  AudioOut = 1u << 0,
  BatteryTelemetry = 1u << 1,
  RfStats = 1u << 2,
  TimingTrace = 1u << 3,
  Arq = 1u << 4,
  Afh = 1u << 5,
  Fec = 1u << 6,
  AdaptivePhy = 1u << 7,
};

constexpr std::uint32_t capabilityMask(Capability capability) {
  return static_cast<std::uint32_t>(capability);
}

constexpr std::uint32_t capabilityMask(Capability first, Capability second) {
  return capabilityMask(first) | capabilityMask(second);
}

enum class FieldId : std::uint8_t {
  DeviceState = 0x01,
  RssiDbm = 0x02,
  CrcGood = 0x03,
  CrcBad = 0x04,
  Missing = 0x05,
  QueueDepth = 0x06,
  MaxQueueDepth = 0x07,
  SchedulerMisses = 0x08,
  IrqToSpiUs = 0x09,
  RxProcessingUs = 0x0A,
  RxRearmUs = 0x0B,
  TraceOverwrites = 0x0C,
  JitterDepth = 0x0D,
  Underruns = 0x0E,
  ArqRetransmitSent = 0x0F,
  ArqRepairUseful = 0x10,
  ArqRepairLate = 0x11,
  AfhMapVersion = 0x12,
  PhyMode = 0x13,
  CapabilityMask = 0x14,
};

struct OptionalMetric {
  bool available = false;
  std::int64_t value = 0;
};

struct Snapshot {
  DeviceState state = DeviceState::Booting;
  std::uint32_t capability_mask = 0;
  instrumentation::Counters counters{};
  std::uint32_t trace_overwrites = 0;
  OptionalMetric rssi_dbm{};
  OptionalMetric queue_depth{};
  OptionalMetric max_queue_depth{};
  OptionalMetric irq_to_spi_us{};
  OptionalMetric rx_processing_us{};
  OptionalMetric rx_rearm_us{};
  OptionalMetric jitter_depth{};
  OptionalMetric underruns{};
  OptionalMetric arq_retransmit_sent{};
  OptionalMetric arq_repair_useful{};
  OptionalMetric arq_repair_late{};
  OptionalMetric afh_map_version{};
  OptionalMetric phy_mode{};
};

struct FieldValue {
  FieldId field;
  std::int64_t value;
};

constexpr const char* deviceStateName(DeviceState state) {
  switch (state) {
    case DeviceState::Booting:
      return "booting";
    case DeviceState::SafeIdle:
      return "safe_idle";
    case DeviceState::Ready:
      return "ready";
    case DeviceState::Streaming:
      return "streaming";
    case DeviceState::Degraded:
      return "degraded";
    case DeviceState::Fault:
      return "fault";
  }
  return "unknown";
}

constexpr const char* fieldName(FieldId field) {
  switch (field) {
    case FieldId::DeviceState:
      return "device_state";
    case FieldId::RssiDbm:
      return "rssi_dbm";
    case FieldId::CrcGood:
      return "crc_good";
    case FieldId::CrcBad:
      return "crc_bad";
    case FieldId::Missing:
      return "missing";
    case FieldId::QueueDepth:
      return "queue_depth";
    case FieldId::MaxQueueDepth:
      return "max_queue_depth";
    case FieldId::SchedulerMisses:
      return "scheduler_misses";
    case FieldId::IrqToSpiUs:
      return "irq_to_spi_us";
    case FieldId::RxProcessingUs:
      return "rx_processing_us";
    case FieldId::RxRearmUs:
      return "rx_rearm_us";
    case FieldId::TraceOverwrites:
      return "trace_overwrites";
    case FieldId::JitterDepth:
      return "jitter_depth";
    case FieldId::Underruns:
      return "underruns";
    case FieldId::ArqRetransmitSent:
      return "arq_retransmit_sent";
    case FieldId::ArqRepairUseful:
      return "arq_repair_useful";
    case FieldId::ArqRepairLate:
      return "arq_repair_late";
    case FieldId::AfhMapVersion:
      return "afh_map_version";
    case FieldId::PhyMode:
      return "phy_mode";
    case FieldId::CapabilityMask:
      return "capability_mask";
  }
  return "unknown";
}

constexpr const char* eventName(instrumentation::Event event) {
  switch (event) {
    case instrumentation::Event::AudioCaptureDone:
      return "audio_capture_done";
    case instrumentation::Event::OpusEncodeStart:
      return "opus_encode_start";
    case instrumentation::Event::OpusEncodeEnd:
      return "opus_encode_end";
    case instrumentation::Event::RadioTxEnqueue:
      return "radio_tx_enqueue";
    case instrumentation::Event::Sx1280TxStart:
      return "sx1280_tx_start";
    case instrumentation::Event::Sx1280TxDone:
      return "sx1280_tx_done";
    case instrumentation::Event::DioRxDone:
      return "dio_rx_done";
    case instrumentation::Event::IsrEnter:
      return "isr_enter";
    case instrumentation::Event::SpiReadStart:
      return "spi_read_start";
    case instrumentation::Event::SpiReadEnd:
      return "spi_read_end";
    case instrumentation::Event::FecRecovered:
      return "fec_recovered";
    case instrumentation::Event::NackSent:
      return "nack_sent";
    case instrumentation::Event::RetransmitSent:
      return "retransmit_sent";
    case instrumentation::Event::PlcUsed:
      return "plc_used";
    case instrumentation::Event::AudioPlayed:
      return "audio_played";
    case instrumentation::Event::SchedulerMiss:
      return "scheduler_miss";
    case instrumentation::Event::RxPacketOk:
      return "rx_packet_ok";
    case instrumentation::Event::RxCrcFail:
      return "rx_crc_fail";
    case instrumentation::Event::RxRearmStart:
      return "rx_rearm_start";
    case instrumentation::Event::RxRearmDone:
      return "rx_rearm_done";
    case instrumentation::Event::QueueDepth:
      return "queue_depth";
  }
  return "unknown";
}

template <typename Emit>
void forEachSnapshotField(const Snapshot& snapshot, Emit&& emit) {
  emit(FieldValue{FieldId::DeviceState, static_cast<std::int64_t>(snapshot.state)});
  if (snapshot.rssi_dbm.available) {
    emit(FieldValue{FieldId::RssiDbm, snapshot.rssi_dbm.value});
  }
  emit(FieldValue{FieldId::CrcGood, static_cast<std::int64_t>(snapshot.counters.crc_good)});
  emit(FieldValue{FieldId::CrcBad, static_cast<std::int64_t>(snapshot.counters.crc_bad)});
  emit(FieldValue{FieldId::Missing, static_cast<std::int64_t>(snapshot.counters.missing)});
  if (snapshot.queue_depth.available) {
    emit(FieldValue{FieldId::QueueDepth, snapshot.queue_depth.value});
  }
  if (snapshot.max_queue_depth.available) {
    emit(FieldValue{FieldId::MaxQueueDepth, snapshot.max_queue_depth.value});
  }
  emit(FieldValue{FieldId::SchedulerMisses,
                  static_cast<std::int64_t>(snapshot.counters.scheduler_misses)});
  if (snapshot.irq_to_spi_us.available) {
    emit(FieldValue{FieldId::IrqToSpiUs, snapshot.irq_to_spi_us.value});
  }
  if (snapshot.rx_processing_us.available) {
    emit(FieldValue{FieldId::RxProcessingUs, snapshot.rx_processing_us.value});
  }
  if (snapshot.rx_rearm_us.available) {
    emit(FieldValue{FieldId::RxRearmUs, snapshot.rx_rearm_us.value});
  }
  emit(FieldValue{FieldId::TraceOverwrites, static_cast<std::int64_t>(snapshot.trace_overwrites)});
  if (snapshot.jitter_depth.available) {
    emit(FieldValue{FieldId::JitterDepth, snapshot.jitter_depth.value});
  }
  if (snapshot.underruns.available) {
    emit(FieldValue{FieldId::Underruns, snapshot.underruns.value});
  }
  if (snapshot.arq_retransmit_sent.available) {
    emit(FieldValue{FieldId::ArqRetransmitSent, snapshot.arq_retransmit_sent.value});
  }
  if (snapshot.arq_repair_useful.available) {
    emit(FieldValue{FieldId::ArqRepairUseful, snapshot.arq_repair_useful.value});
  }
  if (snapshot.arq_repair_late.available) {
    emit(FieldValue{FieldId::ArqRepairLate, snapshot.arq_repair_late.value});
  }
  if (snapshot.afh_map_version.available) {
    emit(FieldValue{FieldId::AfhMapVersion, snapshot.afh_map_version.value});
  }
  if (snapshot.phy_mode.available) {
    emit(FieldValue{FieldId::PhyMode, snapshot.phy_mode.value});
  }
  emit(FieldValue{FieldId::CapabilityMask, static_cast<std::int64_t>(snapshot.capability_mask)});
}

}  // namespace pr1::telemetry
