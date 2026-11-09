#include "online_theme.h"
#include "nvs.h"

bool online_cues_load(void) {
    nvs_handle_t handle;uint8_t value=1;
    if(nvs_open("dsh_voice",NVS_READONLY,&handle)==ESP_OK) {
        if(nvs_get_u8(handle,"cues",&value)!=ESP_OK || value>1)value=1;
        nvs_close(handle);
    }
    return value!=0;
}
bool online_cues_save(bool enabled) {
    nvs_handle_t handle;
    if(nvs_open("dsh_voice",NVS_READWRITE,&handle)!=ESP_OK)return false;
    esp_err_t result=nvs_set_u8(handle,"cues",enabled?1:0);
    if(result==ESP_OK)result=nvs_commit(handle);
    nvs_close(handle);return result==ESP_OK;
}

bool online_overlay_load(void) {
    nvs_handle_t handle;uint8_t value=1;
    if(nvs_open("dsh_voice",NVS_READONLY,&handle)==ESP_OK) {
        if(nvs_get_u8(handle,"overlay",&value)!=ESP_OK || value>1)value=1;
        nvs_close(handle);
    }
    return value!=0;
}
bool online_overlay_save(bool enabled) {
    nvs_handle_t handle;
    if(nvs_open("dsh_voice",NVS_READWRITE,&handle)!=ESP_OK)return false;
    esp_err_t result=nvs_set_u8(handle,"overlay",enabled?1:0);
    if(result==ESP_OK)result=nvs_commit(handle);
    nvs_close(handle);return result==ESP_OK;
}

unsigned online_brightness_load(void) {
    nvs_handle_t handle;uint8_t value=100;
    if(nvs_open("dsh_voice",NVS_READONLY,&handle)==ESP_OK) {
        if(nvs_get_u8(handle,"brightness",&value)!=ESP_OK)value=100;
        nvs_close(handle);
    }
    return online_brightness_normalize(value);
}
bool online_brightness_save(unsigned percent) {
    if(percent!=online_brightness_normalize(percent))return false;
    nvs_handle_t handle;
    if(nvs_open("dsh_voice",NVS_READWRITE,&handle)!=ESP_OK)return false;
    esp_err_t result=nvs_set_u8(handle,"brightness",(uint8_t)percent);
    if(result==ESP_OK)result=nvs_commit(handle);
    nvs_close(handle);return result==ESP_OK;
}

online_idle_t online_idle_load(void) {
    nvs_handle_t handle;
    uint8_t value=ONLINE_IDLE_1_MIN;
    if(nvs_open("dsh_voice",NVS_READONLY,&handle)==ESP_OK) {
        if(nvs_get_u8(handle,"idle",&value)!=ESP_OK)value=ONLINE_IDLE_1_MIN;
        nvs_close(handle);
    }
    return online_idle_normalize(value);
}

bool online_idle_save(online_idle_t setting) {
    if((unsigned)setting>ONLINE_IDLE_NEVER)return false;
    nvs_handle_t handle;
    if(nvs_open("dsh_voice",NVS_READWRITE,&handle)!=ESP_OK)return false;
    esp_err_t result=nvs_set_u8(handle,"idle",(uint8_t)setting);
    if(result==ESP_OK)result=nvs_commit(handle);
    nvs_close(handle);
    return result==ESP_OK;
}

online_theme_t online_theme_load(void) {
    nvs_handle_t handle;
    uint8_t value=ONLINE_THEME_WHALE;
    if(nvs_open("dsh_voice",NVS_READONLY,&handle)==ESP_OK) {
        if(nvs_get_u8(handle,"theme",&value)!=ESP_OK)value=ONLINE_THEME_WHALE;
        nvs_close(handle);
    }
    return online_theme_normalize(value);
}

bool online_theme_save(online_theme_t theme) {
    if(theme!=ONLINE_THEME_WHALE && theme!=ONLINE_THEME_MINT)return false;
    nvs_handle_t handle;
    if(nvs_open("dsh_voice",NVS_READWRITE,&handle)!=ESP_OK)return false;
    esp_err_t result=nvs_set_u8(handle,"theme",(uint8_t)theme);
    if(result==ESP_OK)result=nvs_commit(handle);
    nvs_close(handle);
    return result==ESP_OK;
}
