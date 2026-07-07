#pragma once
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_err.h"
void rx_stats_disconnected(void);
void rx_stats_reconnect_attempt(void);
esp_err_t rx_udp_start(EventGroupHandle_t events, EventBits_t connected_bit);
