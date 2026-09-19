#pragma once

#include "pr1_live_profile.hpp"
#include "pr1_radio_port.hpp"

#if PR1_RF_ENABLED
#include <Arduino.h>
#include <RadioLib.h>
#include <SPI.h>

namespace pr1::runtime {

class Sx1280RadioLibPort final : public RadioPort {
 public:
  Sx1280RadioLibPort();

  bool beginFixedFlrc(const FixedFlrcProfile& profile) override;
  bool startReceive() override;
  RadioReadResult readPacket(std::uint8_t* out,
                             std::size_t capacity,
                             std::size_t* length) override;
  bool transmit(const std::uint8_t* data, std::size_t length) override;
  std::int16_t rssiDbm() override;
  std::uint32_t nowMicros() const override;
  void setRxIrqHandler(RxIrqHandler handler, void* context) override;

 private:
  static void onPacketReceivedStatic();
  void dispatchRxIrq();

  SX1280 radio_;
  RxIrqHandler irq_handler_ = nullptr;
  void* irq_context_ = nullptr;

  static Sx1280RadioLibPort* active_instance_;
};

}  // namespace pr1::runtime
#endif
