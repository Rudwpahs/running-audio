#pragma once

// Code placement (Gate C1, issue #52). The ESP32-S3 runs flash code through one 16 KB
// instruction cache shared by both cores. Measured on board: any rarely-run code
// (control-plane text formatting, map/estimator logic, the USB driver) evicts the RX
// post-read path, the next post-read runs ~100-180 us slower, and at a 150 us gap the
// following frame is lost. Cold adaptive-map / control-plane functions are therefore
// placed in internal IRAM, which bypasses the cache. No behaviour change; no-op on host.
#if defined(ARDUINO_ARCH_ESP32) || defined(ESP_PLATFORM)
#include "esp_attr.h"
#define PR1_IRAM IRAM_ATTR
#else
#define PR1_IRAM
#endif
