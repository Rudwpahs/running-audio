#pragma once

#include <cstdint>

namespace pr1::sequence {

using LogicalFrameIndex = std::uint64_t;

struct LogicalFrameId {
  std::uint32_t session_generation = 0;
  LogicalFrameIndex index = 0;
};

enum class UnwrapStatus : std::uint8_t {
  Ok,
  AmbiguousHalfRange,
  BeforeOrigin,
  Uninitialized,
};

struct UnwrapResult {
  UnwrapStatus status = UnwrapStatus::Uninitialized;
  LogicalFrameIndex index = 0;
  bool forward = false;
};

class SequenceUnwrapper {
 public:
  void reset(std::uint16_t raw_sequence, LogicalFrameIndex logical_index) {
    raw_reference_ = raw_sequence;
    latest_ = logical_index;
    initialized_ = true;
  }

  bool initialized() const { return initialized_; }
  LogicalFrameIndex latest() const { return latest_; }
  std::uint16_t rawReference() const { return raw_reference_; }

  UnwrapResult preview(std::uint16_t raw_sequence) const {
    if (!initialized_) {
      return {UnwrapStatus::Uninitialized, 0, false};
    }

    const std::uint16_t modular =
        static_cast<std::uint16_t>(raw_sequence - raw_reference_);
    if (modular == 0x8000U) {
      return {UnwrapStatus::AmbiguousHalfRange, 0, false};
    }

    const std::int32_t delta =
        modular <= 0x7FFFU
            ? static_cast<std::int32_t>(modular)
            : -static_cast<std::int32_t>(0x10000U - modular);

    if (delta < 0) {
      const auto backwards =
          static_cast<std::uint64_t>(-static_cast<std::int64_t>(delta));
      if (backwards > latest_) {
        return {UnwrapStatus::BeforeOrigin, 0, false};
      }
      return {UnwrapStatus::Ok, latest_ - backwards, false};
    }

    return {UnwrapStatus::Ok,
            latest_ + static_cast<std::uint64_t>(delta),
            delta > 0};
  }

  bool acceptForward(const UnwrapResult& result) {
    if (!initialized_ || result.status != UnwrapStatus::Ok ||
        !result.forward || result.index <= latest_) {
      return false;
    }

    const auto advance = result.index - latest_;
    raw_reference_ = static_cast<std::uint16_t>(
        raw_reference_ + static_cast<std::uint16_t>(advance & 0xFFFFULL));
    latest_ = result.index;
    return true;
  }

 private:
  bool initialized_ = false;
  std::uint16_t raw_reference_ = 0;
  LogicalFrameIndex latest_ = 0;
};

}  // namespace pr1::sequence
