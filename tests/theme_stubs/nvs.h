#pragma once
#include <stdint.h>
#include <stddef.h>
typedef int esp_err_t;
typedef unsigned nvs_handle_t;
typedef enum { NVS_READONLY, NVS_READWRITE } nvs_open_mode_t;
#define ESP_OK 0
#define ESP_FAIL -1
esp_err_t nvs_open(const char *name,nvs_open_mode_t mode,nvs_handle_t *handle);
esp_err_t nvs_get_u8(nvs_handle_t handle,const char *key,uint8_t *value);
esp_err_t nvs_set_u8(nvs_handle_t handle,const char *key,uint8_t value);
esp_err_t nvs_commit(nvs_handle_t handle);
esp_err_t nvs_get_blob(nvs_handle_t handle,const char *key,void *out,size_t *size);
esp_err_t nvs_set_blob(nvs_handle_t handle,const char *key,const void *data,size_t size);
void nvs_close(nvs_handle_t handle);
