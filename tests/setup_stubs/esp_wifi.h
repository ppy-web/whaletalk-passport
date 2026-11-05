#pragma once
#include <stdbool.h>
#include <stdint.h>
typedef int esp_err_t;
#define ESP_OK 0
#define ESP_ERR_INVALID_ARG 1
#define WIFI_SCAN_TYPE_ACTIVE 0
typedef struct { unsigned min,max; } wifi_active_scan_time_t;
typedef struct { wifi_active_scan_time_t active; } wifi_scan_time_t;
typedef struct { int scan_type; wifi_scan_time_t scan_time; unsigned home_chan_dwell_time; } wifi_scan_config_t;
typedef struct { uint8_t ssid[33]; } wifi_ap_record_t;
esp_err_t esp_wifi_scan_start(const wifi_scan_config_t *config,bool block);
esp_err_t esp_wifi_scan_get_ap_records(uint16_t *count,wifi_ap_record_t *records);
esp_err_t esp_wifi_scan_stop(void);
esp_err_t esp_wifi_clear_ap_list(void);
