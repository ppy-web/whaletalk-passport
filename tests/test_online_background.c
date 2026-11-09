#include "online_background.h"
#include "nvs.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static uint8_t stored[ONLINE_BG_BYTES],pending[ONLINE_BG_BYTES];
static size_t stored_size,pending_size;
static uint8_t selected,pending_selected;
static bool fail_open,fail_set,fail_commit,fail_read,changed_image,changed_selection;
static unsigned handles;
esp_err_t nvs_open(const char *name,nvs_open_mode_t mode,nvs_handle_t *h) {
    (void)mode;assert(!strcmp(name,"dsh_voice"));
    if(fail_open)return ESP_FAIL;
    *h=++handles;changed_image=changed_selection=false;return ESP_OK;
}
void nvs_close(nvs_handle_t h){assert(h && handles);handles--;}
esp_err_t nvs_get_u8(nvs_handle_t h,const char *key,uint8_t *value) {
    assert(h && !strcmp(key,"background"));if(fail_read)return ESP_FAIL;*value=selected;return ESP_OK;
}
esp_err_t nvs_set_u8(nvs_handle_t h,const char *key,uint8_t value) {
    assert(h && !strcmp(key,"background"));if(fail_set)return ESP_FAIL;
    pending_selected=value;changed_selection=true;return ESP_OK;
}
esp_err_t nvs_get_blob(nvs_handle_t h,const char *key,void *out,size_t *size) {
    assert(h && !strcmp(key,"bg_custom"));if(fail_read || !stored_size)return ESP_FAIL;
    if(out){if(*size<stored_size)return ESP_FAIL;memcpy(out,stored,stored_size);}
    *size=stored_size;return ESP_OK;
}
esp_err_t nvs_set_blob(nvs_handle_t h,const char *key,const void *data,size_t size) {
    assert(h && !strcmp(key,"bg_custom") && size==ONLINE_BG_BYTES);
    if(fail_set)return ESP_FAIL;
    memcpy(pending,data,size);pending_size=size;changed_image=true;return ESP_OK;
}
esp_err_t nvs_commit(nvs_handle_t h) {
    assert(h);if(fail_commit)return ESP_FAIL;
    if(changed_image){memcpy(stored,pending,pending_size);stored_size=pending_size;}
    if(changed_selection)selected=pending_selected;
    return ESP_OK;
}
int main(void) {
    uint8_t image[ONLINE_BG_BYTES]={'W','T','B','G',1,96,128,16};
    uint16_t output[ONLINE_BG_PIXELS];
    image[8]=0x00;image[9]=0xf8;image[10]=0xe0;image[11]=0x07;
    memset(image+40,0x01,ONLINE_BG_BYTES-40);
    assert(online_background_valid(image,sizeof(image)));
    assert(online_background_decode(image,sizeof(image),output,ONLINE_BG_PIXELS));
    for(unsigned n=0;n<ONLINE_BG_PIXELS;n++)assert(output[n]==(n%2?0x07e0:0xf800));
    uint16_t row[240];
    for(unsigned y=0;y<320;y++) {
        assert(online_background_decode_row(image,sizeof(image),0,y,240,row));
        for(unsigned x=0;x<240;x++)assert(row[x]==output[(y*128/320)*96+x*96/240]);
        assert(online_background_decode_row(image,sizeof(image),3,y,237,row));
        for(unsigned x=0;x<237;x++)assert(row[x]==output[(y*128/320)*96+(x+3)*96/240]);
    }
    assert(!online_background_decode_row(image,sizeof(image),0,320,1,row));
    assert(!online_background_decode_row(image,sizeof(image),239,0,2,row));
    assert(!online_background_decode_row(image,sizeof(image),0,0,0,row));
    assert(!online_background_decode_row(image,sizeof(image),0,0,240,NULL));
    assert(!online_background_decode(image,sizeof(image)-1,output,ONLINE_BG_PIXELS));
    assert(!online_background_decode(image,sizeof(image),output,ONLINE_BG_PIXELS-1));
    assert(!online_background_decode(NULL,sizeof(image),output,ONLINE_BG_PIXELS));
    image[4]=2;assert(!online_background_store(image,sizeof(image)));image[4]=1;
    assert(!handles && !stored_size);
    online_background_init();assert(!online_background_custom_available());
    assert(online_background_load()==ONLINE_BACKGROUND_DAY);
    assert(online_background_save(ONLINE_BACKGROUND_NIGHT));
    assert(online_background_store(image,sizeof(image)));
    assert(online_background_load()==ONLINE_BACKGROUND_NIGHT);
    assert(online_background_save(ONLINE_BACKGROUND_OFF));
    assert(online_background_load()==ONLINE_BACKGROUND_OFF);
    assert(online_background_save(ONLINE_BACKGROUND_NIGHT));
    fail_commit=true;assert(!online_background_save(ONLINE_BACKGROUND_DAY));
    assert(online_background_load()==ONLINE_BACKGROUND_NIGHT);
    uint8_t previous=stored[40];image[40]=0xff;
    assert(!online_background_store(image,sizeof(image)));assert(stored[40]==previous);
    fail_commit=false;fail_set=true;assert(!online_background_store(image,sizeof(image)));assert(stored[40]==previous);
    fail_set=false;fail_open=true;assert(!online_background_store(image,sizeof(image)));
    online_background_init();assert(!online_background_custom_available());
    fail_open=false;fail_read=true;online_background_init();assert(!online_background_custom_available());
    fail_read=false;stored[4]=99;online_background_init();assert(!online_background_custom_available());stored[4]=1;
    online_background_init();assert(online_background_custom_available());
    const uint8_t *loaded=online_background_data();assert(online_background_valid(loaded,ONLINE_BG_BYTES));
    assert(online_background_decode_row(loaded,ONLINE_BG_BYTES,0,0,240,row));assert(row[0]==0xf800 && row[3]==0x07e0);
    online_background_init();assert(loaded==online_background_data());
    assert(!online_background_save((online_background_t)255));selected=255;
    assert(online_background_load()==ONLINE_BACKGROUND_DAY);assert(!handles);
    puts("Background format, palette decoding, persistence, failed replacement, fallback and handle cleanup: PASS");
}
