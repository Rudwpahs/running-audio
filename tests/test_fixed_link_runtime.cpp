#include <algorithm>
#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <vector>

#include "../firmware/common/pr1_packet.hpp"
#include "../firmware/t3s3_sx1280_runtime/include/pr1_fixed_link_runtime.hpp"
#include "../firmware/t3s3_sx1280_runtime/include/pr1_radio_port.hpp"

namespace {

class FakeRadio final : public pr1::runtime::RadioPort {
 public:
  bool beginFixedFlrc(const pr1::runtime::FixedFlrcProfile& profile) override {
    begin_called = true;
    last_profile = profile;
    return begin_ok;
  }

  bool startReceive() override {
    ++rearm_calls;
    clock_us += rearm_cost_us;
    return rearm_ok;
  }

  pr1::runtime::RadioReadResult readPacket(std::uint8_t* out,
                                            std::size_t capacity,
                                            std::size_t* length) override {
    clock_us += read_cost_us;
    if (rx_packets.empty()) return pr1::runtime::RadioReadResult::Error;
    const auto packet = rx_packets.front();
    rx_packets.erase(rx_packets.begin());
    if (packet.bytes.size() > capacity) return pr1::runtime::RadioReadResult::Error;
    std::copy(packet.bytes.begin(), packet.bytes.end(), out);
    *length = packet.bytes.size();
    return packet.result;
  }

  bool transmit(const std::uint8_t* data, std::size_t length) override {
    tx_packets.emplace_back(data, data + length);
    clock_us += tx_cost_us;
    return tx_ok;
  }

  std::int16_t rssiDbm() override { return rssi_dbm; }
  std::int16_t snrDb() override { return snr_db; }
  std::uint32_t nowMicros() const override { return clock_us; }

  void setRxIrqHandler(pr1::runtime::RxIrqHandler handler, void* context) override {
    irq_handler = handler;
    irq_context = context;
  }

  void queueRx(const std::vector<std::uint8_t>& bytes,
               pr1::runtime::RadioReadResult result = pr1::runtime::RadioReadResult::Ok) {
    rx_packets.push_back({bytes, result});
  }

  void triggerRx(std::uint32_t timestamp_us) {
    clock_us = timestamp_us;
    assert(irq_handler != nullptr);
    irq_handler(irq_context, timestamp_us);
  }

  struct RxPacket {
    std::vector<std::uint8_t> bytes;
    pr1::runtime::RadioReadResult result;
  };

  bool begin_ok = true;
  bool rearm_ok = true;
  bool tx_ok = true;
  bool begin_called = false;
  std::uint32_t clock_us = 0;
  std::uint32_t read_cost_us = 80;
  std::uint32_t tx_cost_us = 50;
  std::uint32_t rearm_cost_us = 60;
  std::uint32_t rearm_calls = 0;
  std::int16_t rssi_dbm = -45;
  std::int16_t snr_db = 12;
  pr1::runtime::FixedFlrcProfile last_profile{};
  std::vector<std::vector<std::uint8_t>> tx_packets;
  std::vector<RxPacket> rx_packets;
  pr1::runtime::RxIrqHandler irq_handler = nullptr;
  void* irq_context = nullptr;
};

std::vector<std::uint8_t> makePacket(std::uint16_t sequence,
                                     std::uint16_t stream_id = 1) {
  std::array<std::uint8_t, pr1::kDartTargetOpusPayloadBytes> payload{};
  for (std::size_t i = 0; i < payload.size(); ++i) {
    payload[i] = static_cast<std::uint8_t>((sequence + i) & 0xFFU);
  }
  std::array<std::uint8_t, pr1::kRadioPayloadMaxBytes> encoded{};
  pr1::Header header{};
  header.stream_id = stream_id;
  header.sequence = sequence;
  header.sample_rate = pr1::kDartSampleRateHz;
  const auto length = pr1::encode_packet(
      header, payload.data(), payload.size(), encoded.data(), encoded.size());
  assert(length == pr1::kDartPacketBytes);
  return std::vector<std::uint8_t>(encoded.begin(), encoded.begin() + length);
}

void testTxPostTransmitGapAndCommitSemantics() {
  FakeRadio radio;
  auto profile = pr1::runtime::kFixedFlrcProfile;
  profile.tx_gap_us = 10000U;
  pr1::runtime::FixedLinkRuntime runtime(radio, pr1::runtime::RuntimeRole::Tx, profile, 1U);
  assert(runtime.begin());
  assert(radio.begin_called);

  radio.clock_us = 0U;
  runtime.tick(0U);
  assert(radio.tx_packets.size() == 1U);
  assert(radio.tx_packets[0].size() == pr1::kDartPacketBytes);
  assert(radio.clock_us == 50U);  // blocking TX finished at 50 us in the fake

  pr1::DecodedPacket decoded{};
  assert(pr1::decode_packet(radio.tx_packets[0].data(), radio.tx_packets[0].size(), &decoded));
  assert(decoded.header.sequence == 0U);
  assert(decoded.header.payload_len == pr1::kDartTargetOpusPayloadBytes);

  // Gap is measured from TX completion (50 us), not from TX start (0 us).
  radio.clock_us = 10049U;
  runtime.tick(10049U);
  assert(radio.tx_packets.size() == 1U);

  radio.clock_us = 10050U;
  runtime.tick(10050U);
  assert(radio.tx_packets.size() == 2U);
  assert(radio.clock_us == 10100U);
  assert(pr1::decode_packet(radio.tx_packets[1].data(), radio.tx_packets[1].size(), &decoded));
  assert(decoded.header.sequence == 1U);

  // A failed physical TX must not consume the sequence. We still wait the
  // configured post-attempt gap so a radio fault cannot create a hot retry loop.
  radio.tx_ok = false;
  radio.clock_us = 20100U;
  runtime.tick(20100U);
  assert(radio.clock_us == 20150U);
  assert(pr1::decode_packet(radio.tx_packets[2].data(), radio.tx_packets[2].size(), &decoded));
  assert(decoded.header.sequence == 2U);

  radio.tx_ok = true;
  radio.clock_us = 30150U;
  runtime.tick(30150U);
  assert(pr1::decode_packet(radio.tx_packets[3].data(), radio.tx_packets[3].size(), &decoded));
  assert(decoded.header.sequence == 2U);
}

void testZeroPostTransmitGapIsValid() {
  FakeRadio radio;
  auto profile = pr1::runtime::kFixedFlrcProfile;
  profile.tx_gap_us = 0U;
  pr1::runtime::FixedLinkRuntime runtime(radio, pr1::runtime::RuntimeRole::Tx, profile, 1U);
  assert(runtime.begin());

  runtime.tick(0U);
  assert(radio.tx_packets.size() == 1U);
  assert(radio.clock_us == 50U);

  // Historical V4 includes a 0-us gap point. The next packet may start as soon
  // as the blocking transmit call has completed and the loop services tick().
  runtime.tick(50U);
  assert(radio.tx_packets.size() == 2U);
}

void testRxSequenceAccountingAndRearm() {
  FakeRadio radio;
  pr1::runtime::FixedLinkRuntime runtime(
      radio, pr1::runtime::RuntimeRole::Rx, pr1::runtime::kFixedFlrcProfile, 1U);
  assert(runtime.begin());
  assert(radio.rearm_calls == 1U);

  radio.queueRx(makePacket(100U));
  radio.triggerRx(1000U);
  runtime.tick(1000U);

  auto snapshot = runtime.metrics().snapshot();
  assert(snapshot.crc_good.available && snapshot.crc_good.value == 1);
  assert(snapshot.missing.available && snapshot.missing.value == 0);
  assert(snapshot.max_queue_depth.available && snapshot.max_queue_depth.value == 1);
  assert(snapshot.spi_duration_us.available && snapshot.spi_duration_us.value == 80);
  assert(snapshot.rx_rearm_us.available && snapshot.rx_rearm_us.value == 60);
  assert(radio.rearm_calls == 2U);

  // Duplicate is a valid RF packet but must not create an artificial gap.
  radio.queueRx(makePacket(100U));
  radio.triggerRx(2000U);
  runtime.tick(2000U);
  snapshot = runtime.metrics().snapshot();
  assert(snapshot.crc_good.value == 2);
  assert(snapshot.missing.value == 0);

  // Forward jump 100 -> 102 means exactly one source packet is missing.
  radio.queueRx(makePacket(102U));
  radio.triggerRx(3000U);
  runtime.tick(3000U);
  snapshot = runtime.metrics().snapshot();
  assert(snapshot.crc_good.value == 3);
  assert(snapshot.missing.value == 1);

  // Malformed application packet must be rejected safely and RX must re-arm.
  radio.queueRx(std::vector<std::uint8_t>{0x50, 0x52, 0x01});
  radio.triggerRx(4000U);
  runtime.tick(4000U);
  assert(radio.rearm_calls == 5U);

  // A physical CRC failure is visible separately from a sequence gap.
  radio.queueRx({}, pr1::runtime::RadioReadResult::CrcError);
  radio.triggerRx(5000U);
  runtime.tick(5000U);
  snapshot = runtime.metrics().snapshot();
  assert(snapshot.crc_bad.available && snapshot.crc_bad.value == 1);
}

}  // namespace

int main() {
  testTxPostTransmitGapAndCommitSemantics();
  testZeroPostTransmitGapIsValid();
  testRxSequenceAccountingAndRearm();
  std::cout << "test_fixed_link_runtime: PASS\n";
  return 0;
}
