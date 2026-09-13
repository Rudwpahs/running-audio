#include <cassert>
#include <cstdint>
#include <iostream>

#include "../firmware/t3s3_sx1280_runtime/include/pr1_live_metrics.hpp"

int main() {
  using pr1::runtime::LiveMetrics;

  LiveMetrics metrics;
  auto snapshot = metrics.snapshot();
  assert(snapshot.state == pr1::telemetry::DeviceState::Ready);
  assert(!snapshot.rssi_dbm.available);
  assert(!snapshot.crc_good.available);
  assert(!snapshot.crc_bad.available);
  assert(!snapshot.queue_depth.available);
  assert(!snapshot.irq_to_spi_us.available);
  assert(!snapshot.spi_duration_us.available);
  assert(!snapshot.rx_processing_us.available);
  assert(!snapshot.rx_rearm_us.available);

  // One complete RX lifecycle. Durations are deliberately easy to inspect:
  // IRQ=1000, SPI start=1120, SPI end=1200, packet complete=1280,
  // rearm=1300..1360 us.
  metrics.onRxIrq(1000U, 41U);
  metrics.onSpiStart(1120U, 41U);
  metrics.onSpiEnd(1200U, 41U);
  metrics.onRxPacket(1280U, 41U, -47);
  metrics.onRxRearmStart(1300U, 41U);
  metrics.onRxRearmDone(1360U, 41U);
  metrics.onQueueDepth(0U, 1360U, 41U);
  metrics.onMissing(0U);

  snapshot = metrics.snapshot();
  assert(snapshot.rssi_dbm.available && snapshot.rssi_dbm.value == -47);
  assert(snapshot.crc_good.available && snapshot.crc_good.value == 1);
  assert(snapshot.crc_bad.available && snapshot.crc_bad.value == 0);
  assert(snapshot.missing.available && snapshot.missing.value == 0);
  assert(snapshot.queue_depth.available && snapshot.queue_depth.value == 0);
  assert(snapshot.max_queue_depth.available && snapshot.max_queue_depth.value == 0);
  assert(snapshot.irq_to_spi_us.available && snapshot.irq_to_spi_us.value == 120);
  assert(snapshot.spi_duration_us.available && snapshot.spi_duration_us.value == 80);
  assert(snapshot.rx_processing_us.available && snapshot.rx_processing_us.value == 160);
  assert(snapshot.rx_rearm_us.available && snapshot.rx_rearm_us.value == 60);

  // Observed zero must remain different from never-observed. A CRC failure then
  // makes both good/bad counters host-visible and increments only crc_bad.
  metrics.onRxCrcFail(2000U, 42U);
  snapshot = metrics.snapshot();
  assert(snapshot.crc_good.available && snapshot.crc_good.value == 1);
  assert(snapshot.crc_bad.available && snapshot.crc_bad.value == 1);

  // Queue depth and scheduler misses are live receiver-processing evidence.
  metrics.onQueueDepth(3U, 2100U, 42U);
  metrics.onSchedulerMiss(2200U, 42U);
  snapshot = metrics.snapshot();
  assert(snapshot.queue_depth.value == 3);
  assert(snapshot.max_queue_depth.value == 3);
  assert(snapshot.scheduler_misses.available && snapshot.scheduler_misses.value == 1);

  std::cout << "test_live_metrics: PASS\n";
  return 0;
}
