#include "pr1_sx1280_radiolib.hpp"

#if PR1_RF_ENABLED
#include "../../common/pr1_packet.hpp"
#include "pr1_board_config.hpp"

namespace pr1::runtime {

Sx1280RadioLibPort* Sx1280RadioLibPort::active_instance_ = nullptr;

Sx1280RadioLibPort::Sx1280RadioLibPort()
    : radio_(new Module(board::kSx1280Pins.cs,
                        board::kSx1280Pins.dio1,
                        board::kSx1280Pins.rst,
                        board::kSx1280Pins.busy,
                        SPI)) {}

bool Sx1280RadioLibPort::beginFixedFlrc(const FixedFlrcProfile& profile) {
  SPI.begin(board::kSx1280Pins.sclk,
            board::kSx1280Pins.miso,
            board::kSx1280Pins.mosi,
            board::kSx1280Pins.cs);

  ConfigFLRC_t config;
  config.frequency = profile.frequency_mhz;
  config.bitRate = static_cast<float>(profile.bitrate_kbps);
  config.codingRate = profile.coding_rate;
  config.power = profile.output_dbm;
  config.preambleLength = 16U;
  config.dataShaping = RADIOLIB_SHAPING_0_5;

  const int16_t state = radio_.beginFLRC(config);
  if (state != RADIOLIB_ERR_NONE) return false;

  radio_.setRfSwitchPins(board::kSx1280Pins.rx_enable,
                         board::kSx1280Pins.tx_enable);
  active_instance_ = this;
  return true;
}

bool Sx1280RadioLibPort::startReceive() {
  return radio_.startReceive() == RADIOLIB_ERR_NONE;
}

RadioReadResult Sx1280RadioLibPort::readPacket(std::uint8_t* out,
                                               std::size_t capacity,
                                               std::size_t* length) {
  if (out == nullptr || length == nullptr) return RadioReadResult::Error;

  const std::size_t packet_length = radio_.getPacketLength();
  if (packet_length == 0U || packet_length > capacity) {
    *length = 0U;
    return RadioReadResult::Error;
  }

  const int16_t state = radio_.readData(out, packet_length);
  if (state == RADIOLIB_ERR_CRC_MISMATCH) {
    *length = 0U;
    return RadioReadResult::CrcError;
  }
  if (state != RADIOLIB_ERR_NONE) {
    *length = 0U;
    return RadioReadResult::Error;
  }

  *length = packet_length;
  return RadioReadResult::Ok;
}

bool Sx1280RadioLibPort::transmit(const std::uint8_t* data, std::size_t length) {
  if (data == nullptr || length == 0U || length > pr1::kRadioPayloadMaxBytes) return false;
  return radio_.transmit(data, length) == RADIOLIB_ERR_NONE;
}

std::int16_t Sx1280RadioLibPort::rssiDbm() {
  return static_cast<std::int16_t>(radio_.getRSSI());
}

std::int16_t Sx1280RadioLibPort::snrDb() {
  // SX1280 FLRC packet status does not provide a LoRa-style SNR measurement.
  // The current PR1 telemetry contract therefore does not expose SNR.
  return 0;
}

std::uint32_t Sx1280RadioLibPort::nowMicros() const {
  return static_cast<std::uint32_t>(::micros());
}

void Sx1280RadioLibPort::setRxIrqHandler(RxIrqHandler handler, void* context) {
  irq_handler_ = handler;
  irq_context_ = context;
  active_instance_ = this;
  if (handler != nullptr) {
    radio_.setPacketReceivedAction(&Sx1280RadioLibPort::onPacketReceivedStatic);
  } else {
    radio_.clearPacketReceivedAction();
  }
}

void Sx1280RadioLibPort::onPacketReceivedStatic() {
  if (active_instance_ != nullptr) active_instance_->dispatchRxIrq();
}

void Sx1280RadioLibPort::dispatchRxIrq() {
  if (irq_handler_ != nullptr) {
    irq_handler_(irq_context_, static_cast<std::uint32_t>(::micros()));
  }
}

}  // namespace pr1::runtime
#endif
