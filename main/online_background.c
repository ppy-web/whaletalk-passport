#include "online_background.h"
#include "nvs.h"
#include <stdlib.h>
#include <stdatomic.h>

static _Atomic(uint8_t *) custom_data;
online_background_t online_background_load(void) {
    nvs_handle_t handle;uint8_t value=ONLINE_BACKGROUND_DAY;
    if(nvs_open("dsh_voice",NVS_READONLY,&handle)==ESP_OK) {
        nvs_get_u8(handle,"background",&value);nvs_close(handle);
    }
    return online_background_normalize(value);
}
bool online_background_save(online_background_t background) {
    if((unsigned)background>=ONLINE_BACKGROUND_COUNT)return false;
    nvs_handle_t handle;
    if(nvs_open("dsh_voice",NVS_READWRITE,&handle)!=ESP_OK)return false;
    esp_err_t result=nvs_set_u8(handle,"background",(uint8_t)background);
    if(result==ESP_OK)result=nvs_commit(handle);
    nvs_close(handle);return result==ESP_OK;
}
bool online_background_store(const uint8_t *data,size_t size) {
    if(!online_background_valid(data,size))return false;
    nvs_handle_t handle;
    if(nvs_open("dsh_voice",NVS_READWRITE,&handle)!=ESP_OK)return false;
    /* NVS replaces the blob transactionally; no erase of config or old image. */
    esp_err_t result=nvs_set_blob(handle,"bg_custom",data,size);
    if(result==ESP_OK)result=nvs_commit(handle);
    nvs_close(handle);return result==ESP_OK;
}
void online_background_init(void) {
    if(atomic_load(&custom_data))return;
    nvs_handle_t handle;
    if(nvs_open("dsh_voice",NVS_READONLY,&handle)!=ESP_OK)return;
    size_t size=0;
    if(nvs_get_blob(handle,"bg_custom",NULL,&size)!=ESP_OK || size!=ONLINE_BG_BYTES) {
        nvs_close(handle);return;
    }
    uint8_t *encoded=malloc(ONLINE_BG_BYTES);
    if(!encoded){nvs_close(handle);return;}
    esp_err_t result=nvs_get_blob(handle,"bg_custom",encoded,&size);nvs_close(handle);
    if(result==ESP_OK && online_background_valid(encoded,size))atomic_store(&custom_data,encoded);
    else free(encoded);
}
const uint8_t *online_background_data(void) {return atomic_load(&custom_data);}
bool online_background_custom_available(void) {return online_background_data()!=NULL;}
