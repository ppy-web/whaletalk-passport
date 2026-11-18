#include "online_background_image.h"
#include "online_background.h"
#include "whale_backgrounds.h"
#include "whale_image_scratch.h"
#include "src/draw/lv_image_decoder_private.h"

static lv_image_dsc_t source={
    .header={.magic=LV_IMAGE_HEADER_MAGIC,.cf=LV_COLOR_FORMAT_RAW,.w=240,.h=320},
    .data_size=ONLINE_BG_BYTES,
};
/* C3 uses one LVGL software draw unit. get_area and its immediate draw are
 * synchronous; no row pointers escape into a deferred image task/cache. */
static lv_draw_buf_t scanline;
static bool registered;
static bool supported(const void *src) {
    if(src==&source)return online_background_valid(source.data,source.data_size);
    static const unsigned presets[]={ONLINE_BACKGROUND_DAY,ONLINE_BACKGROUND_NIGHT,
        ONLINE_BACKGROUND_SPRING,ONLINE_BACKGROUND_SUMMER,ONLINE_BACKGROUND_AUTUMN,ONLINE_BACKGROUND_WINTER};
    for(unsigned i=0;i<sizeof(presets)/sizeof(*presets);i++) {
        unsigned n=presets[i];
        const lv_image_dsc_t *preset=whale_backgrounds[n];
        if(src==preset)return preset && preset->data_size>WHALE_BG_PALETTE_BYTES;
    }
    return false;
}
static lv_result_t info(lv_image_decoder_t *decoder,lv_image_decoder_dsc_t *dsc,lv_image_header_t *header) {
    (void)decoder;
    if(!supported(dsc->src))return LV_RESULT_INVALID;
    *header=(lv_image_header_t){.magic=LV_IMAGE_HEADER_MAGIC,.cf=LV_COLOR_FORMAT_RGB565,.w=240,.h=320,.stride=480};
    return LV_RESULT_OK;
}
static lv_result_t open_image(lv_image_decoder_t *decoder,lv_image_decoder_dsc_t *dsc) {
    (void)decoder;
    if(!supported(dsc->src))return LV_RESULT_INVALID;
    dsc->args.no_cache=true;dsc->decoded=NULL;return LV_RESULT_OK;
}
static bool preset_row(const lv_image_dsc_t *image,unsigned x,unsigned y,unsigned width,uint16_t *out) {
    unsigned scale=240/image->header.w;
    const uint8_t *palette=image->data;
    const uint8_t *indices=whale_band_cached_row(palette+WHALE_BG_PALETTE_BYTES,
        image->data_size-WHALE_BG_PALETTE_BYTES,image->header.w,image->header.h,y/scale,&whale_image_scratch.inflate);
    if(!indices)return false;
    for(unsigned n=0;n<width;n++) {
        const uint8_t *entry=palette+3*indices[(x+n)/scale];
        out[n]=(uint16_t)((entry[2]>>3)<<11 | (entry[1]>>2)<<5 | entry[0]>>3);
    }
    return true;
}
static lv_result_t get_area(lv_image_decoder_t *decoder,lv_image_decoder_dsc_t *dsc,
                          const lv_area_t *full,lv_area_t *area) {
    (void)decoder;
    if(area->y1==LV_COORD_MIN){*area=*full;area->y2=area->y1;}
    else {area->y1++;area->y2++;}
    if(area->y1>full->y2)return LV_RESULT_INVALID;
    int width=lv_area_get_width(area);
    if(area->x1<0 || area->x2>=240 || area->y1<0 || area->y1>=320 || width<=0)return LV_RESULT_INVALID;
    lv_draw_buf_t *row=lv_draw_buf_reshape(&scanline,LV_COLOR_FORMAT_RGB565,(uint32_t)width,1,0);
    if(!row)return LV_RESULT_INVALID;
    const lv_image_dsc_t *image=dsc->src;
    unsigned x=(unsigned)area->x1,y=(unsigned)area->y1;
    if(image==&source) {
        if(!online_background_decode_row(image->data,image->data_size,x,y,(unsigned)width,(uint16_t *)row->data))
            return LV_RESULT_INVALID;
    } else if(!preset_row(image,x,y,(unsigned)width,(uint16_t *)row->data)) {
        return LV_RESULT_INVALID;
    }
    dsc->decoded=row;return LV_RESULT_OK;
}
static void close_image(lv_image_decoder_t *decoder,lv_image_decoder_dsc_t *dsc) {
    (void)decoder;dsc->decoded=NULL;
}
void online_background_image_init(void) {
    if(registered)return;
    lv_draw_buf_init(&scanline,240,1,LV_COLOR_FORMAT_RGB565,0,
        whale_image_scratch.pixels,LV_DRAW_BUF_SIZE(240,1,LV_COLOR_FORMAT_RGB565));
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
