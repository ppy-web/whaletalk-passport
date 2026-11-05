#pragma once
#include "esp_wifi.h"

/* Blocking, bounded scan. Caller supplies capacity; always releases driver results. */
esp_err_t online_setup_scan(wifi_ap_record_t *records,uint16_t *count);
