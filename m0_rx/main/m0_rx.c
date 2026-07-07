#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"

#include "esp_err.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "nvs_flash.h"

#include "lwip/ip4_addr.h"

#include "pr1_protocol.h"
#include "rx_udp.h"

#define WIFI_CONNECTED_BIT          BIT0
#define RECONNECT_TASK_STACK_SIZE   3072
#define TASK_PRIORITY               5

static const char *TAG = "pr1_m0_rx";
static EventGroupHandle_t s_wifi_events;
static TaskHandle_t s_reconnect_task;
static esp_netif_t *s_sta_netif;

static void init_nvs(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);
}

static esp_err_t set_static_ipv4(esp_netif_t *netif)
{
    esp_err_t err = esp_netif_dhcpc_stop(netif);
    if (err != ESP_OK && err != ESP_ERR_ESP_NETIF_DHCP_ALREADY_STOPPED) {
        return err;
    }

    esp_netif_ip_info_t ip_info = {0};
    ip_info.ip.addr = ipaddr_addr(PR1_RX_IPV4);
    ip_info.netmask.addr = ipaddr_addr(PR1_NETMASK_IPV4);
    ip_info.gw.addr = ipaddr_addr(PR1_TX_IPV4);
    return esp_netif_set_ip_info(netif, &ip_info);
}

static void reconnect_task(void *arg)
{
    (void)arg;

    while (true) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        vTaskDelay(pdMS_TO_TICKS(1000));

        if ((xEventGroupGetBits(s_wifi_events) & WIFI_CONNECTED_BIT) == 0) {
            const esp_err_t err = esp_wifi_connect();
            if (err != ESP_OK && err != ESP_ERR_WIFI_CONN) {
                ESP_LOGW(TAG, "esp_wifi_connect failed: %s", esp_err_to_name(err));
            }
            rx_stats_reconnect_attempt();
        }
    }
}

static void wifi_event_handler(void *arg,
                               esp_event_base_t event_base,
                               int32_t event_id,
                               void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        const esp_err_t err = esp_wifi_connect();
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "initial esp_wifi_connect failed: %s", esp_err_to_name(err));
        }
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_CONNECTED) {
        const esp_err_t err = set_static_ipv4((esp_netif_t *)arg);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "static IPv4 setup failed: %s", esp_err_to_name(err));
        }
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        xEventGroupClearBits(s_wifi_events, WIFI_CONNECTED_BIT);
        rx_stats_disconnected();
        if (s_reconnect_task != NULL) {
            xTaskNotifyGive(s_reconnect_task);
        }
        ESP_LOGW(TAG, "Wi-Fi disconnected; reconnect scheduled in 1 s");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        const ip_event_got_ip_t *event = event_data;
        ESP_LOGI(TAG, "static IPv4=" IPSTR, IP2STR(&event->ip_info.ip));
        xEventGroupSetBits(s_wifi_events, WIFI_CONNECTED_BIT);
    }
}

static void init_wifi_station(void)
{
    s_wifi_events = xEventGroupCreate();
    ESP_ERROR_CHECK(s_wifi_events == NULL ? ESP_ERR_NO_MEM : ESP_OK);

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    s_sta_netif = esp_netif_create_default_wifi_sta();
    ESP_ERROR_CHECK(s_sta_netif == NULL ? ESP_FAIL : ESP_OK);

    wifi_init_config_t init_config = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&init_config));

    ESP_ERROR_CHECK(xTaskCreate(reconnect_task,
                                "wifi_reconnect",
                                RECONNECT_TASK_STACK_SIZE,
                                NULL,
                                TASK_PRIORITY,
                                &s_reconnect_task) == pdPASS ? ESP_OK : ESP_ERR_NO_MEM);

    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler, s_sta_netif, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_event_handler, s_sta_netif, NULL));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = PR1_WIFI_SSID,
            .password = PR1_WIFI_PASSWORD,
            .scan_method = WIFI_FAST_SCAN,
            .channel = PR1_WIFI_CHANNEL,
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        },
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_set_bandwidth(WIFI_IF_STA, WIFI_BW20));
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));
}

void app_main(void)
{
    init_nvs();
    init_wifi_station();
    ESP_ERROR_CHECK(rx_udp_start(s_wifi_events, WIFI_CONNECTED_BIT));
}
