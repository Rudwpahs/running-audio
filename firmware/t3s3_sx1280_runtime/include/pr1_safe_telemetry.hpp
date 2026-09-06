#pragma once

#include "../../common/pr1_telemetry.hpp"

namespace pr1::runtime {

constexpr telemetry::Snapshot makeSafeTelemetrySnapshot() {
  telemetry::Snapshot snapshot{};
  snapshot.state = telemetry::DeviceState::SafeIdle;
  snapshot.capability_mask = telemetry::capabilityMask(telemetry::Capability::TimingTrace);
  return snapshot;
}

}  // namespace pr1::runtime
