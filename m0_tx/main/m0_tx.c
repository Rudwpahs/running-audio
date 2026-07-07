#include <errno.h>
#include <inttypes.h>
#include <stdbool.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"

#include "esp_err.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "nvs_flash.h"

#include "lwip/inet.h"
#include "lwip/ip4_addr.h"
#include "lwip/sockets.h"

#include "pr1_protocol.h"

#define CLIENT_CONNECTED_BIT BIT0
#define TX_TASK_STACK_SIZE    6144
#define STATS_TASK_STACK_SIZE 4096
#define TASK_PRIORITY         5

static const char *TAG = "pr1_m0_tx";
static EventGroupHandle_t s_wifi_events;
static portMUX_TYPE s_stats_lock = portMUX_INITIALIZER_UNLOCKED;

typedef struct {
    uint64_t packets_sent;
    uint64_t send_errors;
    uint64_t socket_restarts;
    uint64_t deadline_misses;
    uint32_t last_sequence;
    uint32_t connected_stations;
} tx_stats_t;

static tx_stats_t s_stats;

static void stats_station_connected(void)
{
    portENTER_CRITICAL(&s_stats_lock);
    s_stats.connected_stations++;
    portEXIT_CRITICAL(&s_stats_lock);
}

static void stats_station_disconnected(void)
{
    portENTER_CRITICAL(&s_stats_lock);
    if (s_stats.connected_stations > 0u) {
        s_stats.connected_stations--;
    }
    portEXIT_CRITICAL(&s_stats_lock);
}

static void wifi_event_handler(void *arg,
                               esp_event_base_t event_base,
                               int32_t event_id,
                               void *event_data)
{
    (void)arg;
    (void)event_base;

    if (event_id == WIFI_EVENT_AP_STACONNECTED) {
        const wifi_event_ap_staconnected_t *event = event_data;
        stats_station_connected();
        xEventGroupSetBits(s_wifi_events, CLIENT_CONNECTED_BIT);
        ESP_LOGI(TAG, "station " MACSTR " joined, AID=%d",
                 MAC2STR(event->mac), event->aid);
    } else if (event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        const wifi_event_ap_stadisconnected_t *event = event_data;
        stats_station_disconnected();
        xEventGroupClearBits(s_wifi_events, CLIENT_CONNECTED_BIT);
        ESP_LOGW(TAG, "station " MACSTR " left, AID=%d reason=%d",
                 MAC2STR(event->mac), event->aid, event->reason);
    }
}

static void init_nvs(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);
}

static void configure_ap_ipv4(esp_netif_t *ap_netif)
{
    esp_err_t err = esp_netif_dhcps_stop(ap_netif);
    if (err != ESP_OK && err != ESP_ERR_ESP_NETIF_DHCP_ALREADY_STOPPED) {
        ESP_ERROR_CHECK(err);
    }

    esp_netif_ip_info_t ip_info = {0};
    ip_info.ip.addr = ipaddr_addr(PR1_TX_IPV4);
    ip_info.netmask.addr = ipaddr_addr(PR1_NETMASK_IPV4);
    ip_info.gw.addr = ipaddr_addr(PR1_TX_IPV4);
    ESP_ERROR_CHECK(esp_netif_set_ip_info(ap_netif, &ip_info));

    err = esp_netif_dhcps_start(ap_netif);
    if (err != ESP_OK && err != ESP_ERR_ESP_NETIF_DHCP_ALREADY_STARTED) {
        ESP_ERROR_CHECK(err);
    }

    ESP_LOGI(TAG, "SoftAP IPv4=%s netmask=%s", PR1_TX_IPV4, PR1_NETMASK_IPV4);
}

static void init_wifi_softap(void)
{
    s_wifi_events = xEventGroupCreate();
    ESP_ERROR_CHECK(s_wifi_events == NULL ? ESP_ERR_NO_MEM : ESP_OK);

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_t *ap_netif = esp_netif_create_default_wifi_ap();
    ESP_ERROR_CHECK(ap_netif == NULL ? ESP_FAIL : ESP_OK);

    wifi_init_config_t init_config = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&init_config));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler, NULL, NULL));

    wifi_config_t wifi_config = {
        .ap = {
            .ssid = PR1_WIFI_SSID,
            .ssid_len = sizeof(PR1_WIFI_SSID) - 1u,
            .channel = PR1_WIFI_CHANNEL,
            .password = PR1_WIFI_PASSWORD,
            .max_connection = 1,
            .authmode = WIFI_AUTH_WPA2_PSK,
            .ssid_hidden = 0,
            .pmf_cfg = {
                .required = false,
            },
        },
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_set_bandwidth(WIFI_IF_AP, WIFI_BW20));
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));
    configure_ap_ipv4(ap_netif);

    ESP_LOGI(TAG, "SoftAP ready: SSID=%s channel=%u UDP=%u",
             PR1_WIFI_SSID, PR1_WIFI_CHANNEL, PR1_UDP_PORT);
}

static int create_udp_socket(struct sockaddr_in *destination)
{
    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (sock < 0) {
        ESP_LOGE(TAG, "socket() failed: errno=%d", errno);
        return -1;
    }

    struct timeval timeout = {
        .tv_sec = 0,
        .tv_usec = 100000,
    };
    if (setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout)) < 0) {
        ESP_LOGW(TAG, "SO_SNDTIMEO failed: errno=%d", errno);
    }

    memset(destination, 0, sizeof(*destination));
    destination->sin_family = AF_INET;
    destination->sin_port = htons(PR1_UDP_PORT);
    if (inet_pton(AF_INET, PR1_RX_IPV4, &destination->sin_addr) != 1) {
        ESP_LOGE(TAG, "invalid destination IPv4: %s", PR1_RX_IPV4);
        close(sock);
        return -1;
    }

    return sock;
}

static void wait_until_deadline(int64_t deadline_us)
{
    while (true) {
        const int64_t remaining_us = deadline_us - esp_timer_get_time();
        if (remaining_us <= 0) {
            return;
        }

        if (remaining_us > 2000) {
            const TickType_t ticks = pdMS_TO_TICKS((uint32_t)((remaining_us - 1000) / 1000));
            vTaskDelay(ticks > 0 ? ticks : 1);
        } else {
            taskYIELD();
        }
    }
}

static void udp_pattern_tx_task(void *arg)
{
    (void)arg;

    uint8_t packet[PR1_PACKET_SIZE];
    uint32_t sequence = 0;
    int sock = -1;
    struct sockaddr_in destination;
    int64_t next_deadline_us = 0;

    while (true) {
        xEventGroupWaitBits(s_wifi_events,
                            CLIENT_CONNECTED_BIT,
                            pdFALSE,
                            pdTRUE,
                            portMAX_DELAY);

        if (sock < 0) {
            sock = create_udp_socket(&destination);
            if (sock < 0) {
                vTaskDelay(pdMS_TO_TICKS(1000));
                continue;
            }
            portENTER_CRITICAL(&s_stats_lock);
            s_stats.socket_restarts++;
            portEXIT_CRITICAL(&s_stats_lock);
            next_deadline_us = esp_timer_get_time();
        }

        pr1_encode_header(packet,
                          PR1_FLAG_M0,
                          sequence,
                          sequence * PR1_SAMPLES_PER_PACKET);
        pr1_fill_m0_payload(&packet[PR1_HEADER_SIZE], sequence);

        const int sent = sendto(sock,
                                packet,
                                sizeof(packet),
                                0,
                                (const struct sockaddr *)&destination,
                                sizeof(destination));
        if (sent != (int)sizeof(packet)) {
            ESP_LOGE(TAG, "sendto failed/short: sent=%d errno=%d", sent, errno);
            portENTER_CRITICAL(&s_stats_lock);
            s_stats.send_errors++;
            portEXIT_CRITICAL(&s_stats_lock);
            shutdown(sock, 0);
            close(sock);
            sock = -1;
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }

        portENTER_CRITICAL(&s_stats_lock);
        s_stats.packets_sent++;
        s_stats.last_sequence = sequence;
        portEXIT_CRITICAL(&s_stats_lock);

        sequence++;
        next_deadline_us += PR1_PACKET_INTERVAL_US;
        const int64_t now_us = esp_timer_get_time();
        if (now_us - next_deadline_us > (2 * PR1_PACKET_INTERVAL_US)) {
            portENTER_CRITICAL(&s_stats_lock);
            s_stats.deadline_misses++;
            portEXIT_CRITICAL(&s_stats_lock);
            next_deadline_us = now_us + PR1_PACKET_INTERVAL_US;
        }
        wait_until_deadline(next_deadline_us);
    }
}

static void stats_task(void *arg)
{
    (void)arg;

    uint64_t previous_sent = 0;
    while (true) {
        vTaskDelay(pdMS_TO_TICKS(1000));

        tx_stats_t snapshot;
        portENTER_CRITICAL(&s_stats_lock);
        snapshot = s_stats;
        portEXIT_CRITICAL(&s_stats_lock);

        const uint64_t packets_per_second = snapshot.packets_sent - previous_sent;
        previous_sent = snapshot.packets_sent;

        ESP_LOGI(TAG,
                 "pps=%" PRIu64 " sent=%" PRIu64 " errors=%" PRIu64
                 " socket_restarts=%" PRIu64 " deadline_misses=%" PRIu64
                 " last_seq=%" PRIu32 " stations=%" PRIu32,
                 packets_per_second,
                 snapshot.packets_sent,
                 snapshot.send_errors,
                 snapshot.socket_restarts,
                 snapshot.deadline_misses,
                 snapshot.last_sequence,
                 snapshot.connected_stations);
    }
}

void app_main(void)
{
    init_nvs();
    init_wifi_softap();

    ESP_ERROR_CHECK(xTaskCreate(udp_pattern_tx_task,
                                "udp_pattern_tx",
                                TX_TASK_STACK_SIZE,
                                NULL,
                                TASK_PRIORITY,
                                NULL) == pdPASS ? ESP_OK : ESP_ERR_NO_MEM);
    ESP_ERROR_CHECK(xTaskCreate(stats_task,
                                "tx_stats",
                                STATS_TASK_STACK_SIZE,
                                NULL,
                                TASK_PRIORITY - 1,
                                NULL) == pdPASS ? ESP_OK : ESP_ERR_NO_MEM);
}
