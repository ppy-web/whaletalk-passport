#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "online_setup_scan.h"

static int scan_result,read_result,starts,reads,stops,clears;
static uint16_t available,expected_capacity;
esp_err_t esp_wifi_scan_start(const wifi_scan_config_t *config,bool block) {
    starts++;
    assert(block && config->scan_type==WIFI_SCAN_TYPE_ACTIVE);
    assert(config->scan_time.active.max<=120 && config->home_chan_dwell_time>=30);
    return scan_result;
}
esp_err_t esp_wifi_scan_get_ap_records(uint16_t *count,wifi_ap_record_t *records) {
    reads++;assert(*count==expected_capacity);
    *count=available<*count?available:*count;
    for(unsigned i=0;i<*count;i++)memset(records[i].ssid,'a',sizeof(records[i].ssid));
    return read_result;
}
esp_err_t esp_wifi_scan_stop(void) { stops++;return ESP_OK; }
esp_err_t esp_wifi_clear_ap_list(void) { clears++;return ESP_OK; }
static void reset(void) { scan_result=read_result=starts=reads=stops=clears=0;available=4;expected_capacity=2; }
int main(void) {
    wifi_ap_record_t records[2];uint16_t count;
    reset();count=2;
    assert(online_setup_scan(records,&count)==ESP_OK && count==2);
    assert(starts==1 && reads==1 && !stops && !clears);
    assert(strlen((char *)records[0].ssid)==32);
    reset();count=2;available=0;
    assert(online_setup_scan(records,&count)==ESP_OK && count==0 && reads==1);
    reset();count=2;scan_result=23;
    assert(online_setup_scan(records,&count)==23 && count==0);
    assert(!reads && stops==1 && clears==1);
    /* A timeout/failure must not leave results or block a subsequent retry. */
    scan_result=0;count=2;
    assert(online_setup_scan(records,&count)==ESP_OK && count==2 && reads==1);
    reset();count=2;read_result=24;
    assert(online_setup_scan(records,&count)==24 && count==0 && stops==1 && clears==1);
    reset();count=0;
    assert(online_setup_scan(records,&count)==ESP_ERR_INVALID_ARG);
    assert(online_setup_scan(NULL,&count)==ESP_ERR_INVALID_ARG);
    assert(online_setup_scan(records,NULL)==ESP_ERR_INVALID_ARG && !starts);
    puts("Setup scan tests: PASS");
}
