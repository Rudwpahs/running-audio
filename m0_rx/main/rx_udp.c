#include "rx_udp.h"

#include <errno.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>

#include "freertos/task.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "lwip/inet.h"
#include "lwip/sockets.h"
#include "pr1_protocol.h"

#define RX_TASK_STACK_SIZE       7168
#define STATS_TASK_STACK_SIZE    4096
#define TASK_PRIORITY            5
#define SEQUENCE_WINDOW_BITS     64u

static const char *TAG = "pr1_m0_rx_udp";
static portMUX_TYPE s_stats_lock = portMUX_INITIALIZER_UNLOCKED;
static EventGroupHandle_t s_wifi_events;
static EventBits_t s_connected_bit;

typedef struct {
    uint64_t total_datagrams;
    uint64_t received_unique;
    uint64_t lost_packets;
    uint64_t duplicate_packets;
    uint64_t out_of_order_packets;
    uint64_t payload_errors;
    uint64_t header_errors;
    uint64_t bad_size_packets;
    uint64_t socket_restarts;
    uint64_t wifi_disconnects;
    uint64_t wifi_reconnects;
} rx_stats_t;

static rx_stats_t s_stats;

void rx_stats_disconnected(void)
{
    portENTER_CRITICAL(&s_stats_lock);
    s_stats.wifi_disconnects++;
    portEXIT_CRITICAL(&s_stats_lock);
}

void rx_stats_reconnect_attempt(void)
{
    portENTER_CRITICAL(&s_stats_lock);
    s_stats.wifi_reconnects++;
    portEXIT_CRITICAL(&s_stats_lock);
}

static int create_bound_socket(void)
{
    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (sock < 0) {
        ESP_LOGE(TAG, "socket() failed: errno=%d", errno);
        return -1;
    }

    int reuse = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
        ESP_LOGW(TAG, "SO_REUSEADDR failed: errno=%d", errno);
    }

    struct timeval timeout = {
        .tv_sec = 1,
        .tv_usec = 0,
    };
    if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0) {
        ESP_LOGW(TAG, "SO_RCVTIMEO failed: errno=%d", errno);
    }

    struct sockaddr_in local_address = {
        .sin_family = AF_INET,
        .sin_port = htons(PR1_UDP_PORT),
        .sin_addr.s_addr = htonl(INADDR_ANY),
    };

    if (bind(sock, (const struct sockaddr *)&local_address, sizeof(local_address)) < 0) {
        ESP_LOGE(TAG, "bind() failed: errno=%d", errno);
        close(sock);
        return -1;
    }
    return sock;
}

typedef struct {
    bool initialized;
    uint32_t highest_sequence;
    uint64_t seen_bitmap;
} sequence_tracker_t;

static void account_sequence(sequence_tracker_t *tracker, uint32_t sequence)
{
    portENTER_CRITICAL(&s_stats_lock);

    if (!tracker->initialized) {
        tracker->initialized = true;
        tracker->highest_sequence = sequence;
        tracker->seen_bitmap = 1u;
        s_stats.received_unique++;
    } else if (pr1_seq_after(sequence, tracker->highest_sequence)) {
        const uint32_t advance = sequence - tracker->highest_sequence;
        if (advance > 1u) {
            s_stats.lost_packets += (uint64_t)(advance - 1u);
        }
        tracker->seen_bitmap = advance >= SEQUENCE_WINDOW_BITS
                                   ? 1u
                                   : (tracker->seen_bitmap << advance) | 1u;
        tracker->highest_sequence = sequence;
        s_stats.received_unique++;
    } else {
        const uint32_t age = tracker->highest_sequence - sequence;
        if (age < SEQUENCE_WINDOW_BITS) {
            const uint64_t mask = UINT64_C(1) << age;
            if ((tracker->seen_bitmap & mask) != 0u) {
                s_stats.duplicate_packets++;
            } else {
                tracker->seen_bitmap |= mask;
                s_stats.out_of_order_packets++;
                s_stats.received_unique++;
            }
        } else {
            s_stats.out_of_order_packets++;
        }
    }

    portEXIT_CRITICAL(&s_stats_lock);
}

static void udp_pattern_rx_task(void *arg)
{
    (void)arg;
    uint8_t packet[PR1_PACKET_SIZE];

    while (true) {
        xEventGroupWaitBits(s_wifi_events, s_connected_bit,
                            pdFALSE, pdTRUE, portMAX_DELAY);

        sequence_tracker_t tracker = {0};
        int sock = create_bound_socket();
        if (sock < 0) {
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }

        portENTER_CRITICAL(&s_stats_lock);
        s_stats.socket_restarts++;
        portEXIT_CRITICAL(&s_stats_lock);
        ESP_LOGI(TAG, "UDP listener bound to port %u", PR1_UDP_PORT);

        while ((xEventGroupGetBits(s_wifi_events) & s_connected_bit) != 0) {
            struct sockaddr_in source_address;
            socklen_t source_length = sizeof(source_address);
            const int received = recvfrom(sock, packet, sizeof(packet), 0,
                                          (struct sockaddr *)&source_address,
                                          &source_length);
            if (received < 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    continue;
                }
                ESP_LOGE(TAG, "recvfrom failed: errno=%d", errno);
                break;
            }

            portENTER_CRITICAL(&s_stats_lock);
            s_stats.total_datagrams++;
            portEXIT_CRITICAL(&s_stats_lock);

            if (received != (int)PR1_PACKET_SIZE) {
                portENTER_CRITICAL(&s_stats_lock);
                s_stats.bad_size_packets++;
                portEXIT_CRITICAL(&s_stats_lock);
                continue;
            }

            pr1_packet_header_t header;
            if (pr1_decode_header(packet, (size_t)received,
                                  PR1_FLAG_M0, &header) != PR1_PACKET_OK) {
                portENTER_CRITICAL(&s_stats_lock);
                s_stats.header_errors++;
                portEXIT_CRITICAL(&s_stats_lock);
                continue;
            }

            size_t bad_offset = SIZE_MAX;
            if (!pr1_validate_m0_payload(&packet[PR1_HEADER_SIZE],
                                         PR1_PAYLOAD_SIZE,
                                         header.sequence,
                                         &bad_offset)) {
                portENTER_CRITICAL(&s_stats_lock);
                s_stats.payload_errors++;
                portEXIT_CRITICAL(&s_stats_lock);
                ESP_LOGW(TAG, "payload mismatch seq=%" PRIu32 " offset=%u",
                         header.sequence, (unsigned)bad_offset);
                continue;
            }

            account_sequence(&tracker, header.sequence);
        }

        shutdown(sock, 0);
        close(sock);
        ESP_LOGW(TAG, "UDP socket closed; waiting for Wi-Fi");
    }
}

static double loss_percent(uint64_t lost, uint64_t unique)
{
    const uint64_t denominator = lost + unique;
    return denominator == 0u ? 0.0 : ((double)lost * 100.0) / (double)denominator;
}

static void stats_task(void *arg)
{
    (void)arg;
    uint64_t previous_total = 0;
    uint64_t previous_unique = 0;
    uint64_t previous_lost = 0;

    while (true) {
        vTaskDelay(pdMS_TO_TICKS(1000));

        rx_stats_t snapshot;
        portENTER_CRITICAL(&s_stats_lock);
        snapshot = s_stats;
        portEXIT_CRITICAL(&s_stats_lock);

        const uint64_t interval_total = snapshot.total_datagrams - previous_total;
        const uint64_t interval_unique = snapshot.received_unique - previous_unique;
        const uint64_t interval_lost = snapshot.lost_packets - previous_lost;
        previous_total = snapshot.total_datagrams;
        previous_unique = snapshot.received_unique;
        previous_lost = snapshot.lost_packets;

        int rssi = 0;
        wifi_ap_record_t ap_info;
        if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) {
            rssi = ap_info.rssi;
        }

        ESP_LOGI(TAG,
                 "pps=%" PRIu64 " unique=%" PRIu64 " lost=%" PRIu64
                 " dup=%" PRIu64 " ooo=%" PRIu64 " payload_err=%" PRIu64
                 " header_err=%" PRIu64 " size_err=%" PRIu64
                 " loss_1s=%.4f%% loss_total=%.4f%% disconnects=%" PRIu64
                 " reconnects=%" PRIu64 " socket_restarts=%" PRIu64 " rssi=%d",
                 interval_total, snapshot.received_unique, snapshot.lost_packets,
                 snapshot.duplicate_packets, snapshot.out_of_order_packets,
                 snapshot.payload_errors, snapshot.header_errors,
                 snapshot.bad_size_packets,
                 loss_percent(interval_lost, interval_unique),
                 loss_percent(snapshot.lost_packets, snapshot.received_unique),
                 snapshot.wifi_disconnects, snapshot.wifi_reconnects,
                 snapshot.socket_restarts, rssi);
    }
}

esp_err_t rx_udp_start(EventGroupHandle_t events, EventBits_t connected_bit)
{
    if (events == NULL || connected_bit == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    s_wifi_events = events;
    s_connected_bit = connected_bit;

    if (xTaskCreate(udp_pattern_rx_task, "udp_pattern_rx",
                    RX_TASK_STACK_SIZE, NULL, TASK_PRIORITY, NULL) != pdPASS) {
        return ESP_ERR_NO_MEM;
    }
    if (xTaskCreate(stats_task, "rx_stats", STATS_TASK_STACK_SIZE,
                    NULL, TASK_PRIORITY - 1, NULL) != pdPASS) {
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}
