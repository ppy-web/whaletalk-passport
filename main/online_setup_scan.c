#include "online_setup_scan.h"

esp_err_t online_setup_scan(wifi_ap_record_t *records,uint16_t *count) {
    if(!records || !count || !*count)return ESP_ERR_INVALID_ARG;
    uint16_t capacity=*count;
    *count=0;
    const wifi_scan_config_t config={
        .scan_type=WIFI_SCAN_TYPE_ACTIVE,
        .scan_time.active={.min=0,.max=120},
        .home_chan_dwell_time=30
    };
    esp_err_t result=esp_wifi_scan_start(&config,true);
    if(result==ESP_OK) {
        result=esp_wifi_scan_get_ap_records(&capacity,records);
        if(result==ESP_OK) {
            *count=capacity;
            for(unsigned i=0;i<capacity;i++)records[i].ssid[32]=0;
            return ESP_OK;
        }
    }
    /* Includes timeout and retrieval failures, even when the client disconnected. */
    esp_wifi_scan_stop();
    esp_wifi_clear_ap_list();
    return result;
}
