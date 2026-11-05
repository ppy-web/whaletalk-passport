#include "online_background_image.h"
#include "online_background.h"
#include "src/draw/lv_image_decoder_private.h"

static lv_image_dsc_t source={
    .header={.magic=LV_IMAGE_HEADER_MAGIC,.cf=LV_COLOR_FORMAT_RAW,.w=240,.h=320},
    .data_size=ONLINE_BG_BYTES,
};
/* C3 uses one LVGL software draw unit. get_area and its immediate draw are
 * synchronous; no row pointers escape into a deferred image task/cache. */
LV_DRAW_BUF_DEFINE_STATIC(scanline,240,1,LV_COLOR_FORMAT_RGB565);
static bool registered;
static lv_result_t info(lv_image_decoder_t *decoder,lv_image_decoder_dsc_t *dsc,lv_image_header_t *header) {
    (void)decoder;
    if(dsc->src!=&source || !online_background_valid(source.data,source.data_size))return LV_RESULT_INVALID;
    *header=(lv_image_header_t){.magic=LV_IMAGE_HEADER_MAGIC,.cf=LV_COLOR_FORMAT_RGB565,.w=240,.h=320,.stride=480};
    return LV_RESULT_OK;
}
static lv_result_t open_image(lv_image_decoder_t *decoder,lv_image_decoder_dsc_t *dsc) {
    (void)decoder;
    if(dsc->src!=&source || !online_background_valid(source.data,source.data_size))return LV_RESULT_INVALID;
    dsc->args.no_cache=true;dsc->decoded=NULL;return LV_RESULT_OK;
}
static lv_result_t get_area(lv_image_decoder_t *decoder,lv_image_decoder_dsc_t *dsc,
                          const lv_area_t *full,lv_area_t *area) {
    (void)decoder;
    if(area->y1==LV_COORD_MIN){*area=*full;area->y2=area->y1;}
    else {area->y1++;area->y2++;}
    if(area->y1>full->y2)return LV_RESULT_INVALID;
    int width=lv_area_get_width(area);
    if(area->x1<0 || area->y1<0 || width<=0 || width>240)return LV_RESULT_INVALID;
    lv_draw_buf_t *row=lv_draw_buf_reshape(&scanline,LV_COLOR_FORMAT_RGB565,(uint32_t)width,1,0);
    if(!row || !online_background_decode_row(source.data,source.data_size,(unsigned)area->x1,
        (unsigned)area->y1,(unsigned)width,(uint16_t *)row->data))return LV_RESULT_INVALID;
    dsc->decoded=row;return LV_RESULT_OK;
}
static void close_image(lv_image_decoder_t *decoder,lv_image_decoder_dsc_t *dsc) {
    (void)decoder;dsc->decoded=NULL;
}
void online_background_image_init(void) {
    if(registered)return;
    LV_DRAW_BUF_INIT_STATIC(scanline);
    lv_image_decoder_t *decoder=lv_image_decoder_create();
    if(!decoder)return;
    lv_image_decoder_set_info_cb(decoder,info);lv_image_decoder_set_open_cb(decoder,open_image);
    lv_image_decoder_set_get_area_cb(decoder,get_area);lv_image_decoder_set_close_cb(decoder,close_image);
    registered=true;
}
const lv_image_dsc_t *online_background_image(const uint8_t *encoded) {
    if(!registered || !online_background_valid(encoded,ONLINE_BG_BYTES))return NULL;
    source.data=encoded;return &source;
}
