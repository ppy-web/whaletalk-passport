#include "whale_avatar_view.h"
#include "src/draw/lv_image_decoder_private.h"

static bool registered;
/* Single software draw unit, same synchronous get-area contract as custom
 * backgrounds. RGB565 and A8 planes share one native, sub-kilobyte row. */
static LV_ATTRIBUTE_MEM_ALIGN uint8_t scanline_bytes[WHALE_VIEW_WIDTH*3+LV_DRAW_BUF_ALIGN];
static lv_draw_buf_t scanline;

static const lv_image_dsc_t *frame_for(const void *source) {
    for(unsigned n=0;n<ONLINE_AVATAR_COUNT;n++)if(source==whale_avatar_frames[n])return whale_avatar_frames[n];
    return NULL;
}
static lv_result_t info(lv_image_decoder_t *decoder,lv_image_decoder_dsc_t *dsc,lv_image_header_t *header) {
    (void)decoder;
    if(!frame_for(dsc->src))return LV_RESULT_INVALID;
    *header=(lv_image_header_t){.magic=LV_IMAGE_HEADER_MAGIC,.cf=LV_COLOR_FORMAT_RGB565A8,
        .w=WHALE_VIEW_WIDTH,.h=WHALE_VIEW_HEIGHT,.stride=WHALE_VIEW_WIDTH*2};
    return LV_RESULT_OK;
}
static lv_result_t open_image(lv_image_decoder_t *decoder,lv_image_decoder_dsc_t *dsc) {
    (void)decoder;
    const lv_image_dsc_t *frame=frame_for(dsc->src);
    if(!frame || frame->header.cf!=LV_COLOR_FORMAT_RAW ||
        frame->data_size!=WHALE_AVATAR_PALETTE_BYTES+WHALE_VIEW_WIDTH*WHALE_VIEW_HEIGHT)return LV_RESULT_INVALID;
    dsc->user_data=(void *)frame;dsc->args.no_cache=true;dsc->decoded=NULL;
    return LV_RESULT_OK;
}
static lv_result_t get_area(lv_image_decoder_t *decoder,lv_image_decoder_dsc_t *dsc,
                          const lv_area_t *full,lv_area_t *area) {
    (void)decoder;
    if(area->y1==LV_COORD_MIN){*area=*full;area->y2=area->y1;}
    else {area->y1++;area->y2++;}
    if(area->y1>full->y2)return LV_RESULT_INVALID;
    int width=lv_area_get_width(area);
    if(!dsc->user_data || area->x1<0 || area->x2>=WHALE_VIEW_WIDTH || area->y1<0 ||
       area->y1>=WHALE_VIEW_HEIGHT || width<=0)return LV_RESULT_INVALID;
    /* RGB565A8 uses a two-byte color stride and a separate one-byte A8 plane. */
    lv_draw_buf_t *row=lv_draw_buf_reshape(&scanline,LV_COLOR_FORMAT_RGB565A8,(unsigned)width,1,(unsigned)width*2);
    if(!row)return LV_RESULT_INVALID;
    const lv_image_dsc_t *frame=dsc->user_data;
    const uint8_t *indices=frame->data+WHALE_AVATAR_PALETTE_BYTES+
        (unsigned)area->y1*WHALE_VIEW_WIDTH+(unsigned)area->x1;
    uint8_t *mask=row->data+row->header.stride;
    for(unsigned n=0;n<(unsigned)width;n++) {
        const uint8_t *entry=frame->data+indices[n]*3;
        row->data[n*2]=entry[0];row->data[n*2+1]=entry[1];mask[n]=entry[2];
    }
    dsc->decoded=row;return LV_RESULT_OK;
}
static void close_image(lv_image_decoder_t *decoder,lv_image_decoder_dsc_t *dsc) {
    (void)decoder;dsc->user_data=NULL;dsc->decoded=NULL;
}
void whale_avatar_view_init(void) {
    if(registered)return;
    lv_draw_buf_init(&scanline,WHALE_VIEW_WIDTH,1,LV_COLOR_FORMAT_RGB565A8,
        WHALE_VIEW_WIDTH*2,scanline_bytes,sizeof(scanline_bytes));
    lv_image_decoder_t *decoder=lv_image_decoder_create();
    if(!decoder)return;
    lv_image_decoder_set_info_cb(decoder,info);lv_image_decoder_set_open_cb(decoder,open_image);
    lv_image_decoder_set_get_area_cb(decoder,get_area);lv_image_decoder_set_close_cb(decoder,close_image);
    registered=true;
}
const lv_image_dsc_t *whale_avatar_view_source(online_avatar_t frame) {
    if((unsigned)frame>=ONLINE_AVATAR_COUNT)frame=ONLINE_AVATAR_IDLE;
    return registered?whale_avatar_frames[frame]:NULL;
}
