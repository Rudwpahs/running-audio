idf_component_register(SRCS "m0_rx.c" "rx_udp.c" INCLUDE_DIRS "." REQUIRES esp_wifi esp_event esp_netif nvs_flash lwip pr1_protocol)
