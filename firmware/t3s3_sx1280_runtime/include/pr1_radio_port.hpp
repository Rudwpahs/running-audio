#pragma once

#include <cstddef>
#include <cstdint>

#include "pr1_live_profile.hpp"

namespace pr1::runtime {

enum class RadioReadResult : std::uint8_t {
  Ok,
  CrcError,
  Error,
};

using RxIrqHandler = void (*)(void* context, std::uint32_t timestamp_us);

class RadioPort {
 public:
  virtual ~RadioPort() = default;

  virtual bool beginFixedFlrc(const FixedFlrcProfile& profile) = 0;
  virtual bool startReceive() = 0;
  virtual RadioReadResult readPacket(std::uint8_t* out,
                                     std::size_t capacity,
                                     std::size_t* length) = 0;
  virtual bool transmit(const std::uint8_t* data, std::size_t length) = 0;
  virtual std::int16_t rssiDbm() const = 0;
  virtual std::int16_t snrDb() const = 0;
  virtual std::uint32_t nowMicros() const = 0;
  virtual void setRxIrqHandler(RxIrqHandler handler, void* context) = 0;
};

}  // namespace pr1::runtime
