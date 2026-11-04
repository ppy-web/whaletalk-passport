#include "online_theme.h"
#include "nvs.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static bool present[2],fail_open,fail_set,fail_commit;
static uint8_t stored[2],pending;
static int pending_key;
static unsigned open_handles;
static int key_index(const char *key) {
    if(!strcmp(key,"theme"))return 0;
    if(!strcmp(key,"idle"))return 1;
    return -1;
}

esp_err_t nvs_open(const char *name,nvs_open_mode_t mode,nvs_handle_t *handle) {
    (void)mode;
    if(fail_open || strcmp(name,"dsh_voice"))return ESP_FAIL;
    *handle=++open_handles;return ESP_OK;
}
esp_err_t nvs_get_u8(nvs_handle_t handle,const char *key,uint8_t *value) {
    assert(handle && open_handles);
    int index=key_index(key);
    if(index<0 || !present[index])return ESP_FAIL;
    *value=stored[index];return ESP_OK;
}
esp_err_t nvs_set_u8(nvs_handle_t handle,const char *key,uint8_t value) {
    assert(handle && open_handles);
    pending_key=key_index(key);
    if(fail_set || pending_key<0)return ESP_FAIL;
    pending=value;return ESP_OK;
}
esp_err_t nvs_commit(nvs_handle_t handle) {
    assert(handle && open_handles);
    if(fail_commit)return ESP_FAIL;
    stored[pending_key]=pending;present[pending_key]=true;return ESP_OK;
}
void nvs_close(nvs_handle_t handle) { assert(handle && open_handles);open_handles--; }

int main(void) {
    assert(online_theme_load()==ONLINE_THEME_WHALE);
    assert(online_theme_save(ONLINE_THEME_MINT));
    assert(online_theme_load()==ONLINE_THEME_MINT);
    assert(online_theme_save(ONLINE_THEME_WHALE));
    assert(online_theme_load()==ONLINE_THEME_WHALE);
    fail_commit=true;
    assert(!online_theme_save(ONLINE_THEME_MINT));
    assert(online_theme_load()==ONLINE_THEME_WHALE);
    fail_commit=false;fail_set=true;
    assert(!online_theme_save(ONLINE_THEME_MINT));
    assert(online_theme_load()==ONLINE_THEME_WHALE);
    fail_set=false;fail_open=true;
    assert(!online_theme_save(ONLINE_THEME_MINT));
    assert(online_theme_load()==ONLINE_THEME_WHALE);
    fail_open=false;stored[0]=255;present[0]=true;
    assert(online_theme_load()==ONLINE_THEME_WHALE);
    assert(!online_theme_save((online_theme_t)255));
    assert(online_idle_load()==ONLINE_IDLE_1_MIN);
    assert(online_theme_save(ONLINE_THEME_MINT));
    for(unsigned i=0;i<=ONLINE_IDLE_NEVER;i++) {
        assert(online_idle_save((online_idle_t)i));
        assert(online_idle_load()==(online_idle_t)i);
        assert(online_theme_load()==ONLINE_THEME_MINT);
    }
    assert(online_theme_save(ONLINE_THEME_WHALE));
    assert(online_idle_load()==ONLINE_IDLE_NEVER);
    fail_commit=true;assert(!online_idle_save(ONLINE_IDLE_3_MIN));
    assert(online_idle_load()==ONLINE_IDLE_NEVER);
    fail_commit=false;fail_set=true;assert(!online_idle_save(ONLINE_IDLE_3_MIN));
    assert(online_idle_load()==ONLINE_IDLE_NEVER);
    fail_set=false;fail_open=true;assert(!online_idle_save(ONLINE_IDLE_3_MIN));
    assert(online_idle_load()==ONLINE_IDLE_1_MIN);
    fail_open=false;stored[1]=255;present[1]=true;
    assert(online_idle_load()==ONLINE_IDLE_1_MIN);
    assert(!online_idle_save((online_idle_t)255));
    assert(!open_handles);
    puts("Theme persistence, fallback, failure handling and handle cleanup: PASS");
}
