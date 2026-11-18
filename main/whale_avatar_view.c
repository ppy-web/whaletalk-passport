#include "whale_avatar_view.h"
#include "home_foreground.h"
#include "whale_image_scratch.h"
#include "src/draw/lv_image_decoder_private.h"

static bool registered;
/* Single software draw unit, same synchronous get-area contract as custom
 * backgrounds. RGB565 and A8 planes share one native, sub-kilobyte row. */
LV_ATTRIBUTE_MEM_ALIGN whale_image_scratch_t whale_image_scratch;
static lv_draw_buf_t scanline;
static const lv_image_dsc_t *const *const portrait_frames[ONLINE_THEME_COUNT]={
    [ONLINE_THEME_WHALE]=whale_avatar_frames,
    [ONLINE_THEME_WHALE_MAID]=whale_maid_avatar_frames,
    [ONLINE_THEME_CLAUDE]=claude_avatar_frames,
    [ONLINE_THEME_CHATGPT]=chatgpt_avatar_frames,
};
static const lv_image_dsc_t *frame_for(const void *source) {
    if(source==&home_foreground_top || source==&home_foreground_bottom ||
       source==&claude_foreground_top || source==&claude_foreground_bottom ||
       source==&chatgpt_foreground_top || source==&chatgpt_foreground_bottom)return source;
    for(unsigned theme=0;theme<ONLINE_THEME_COUNT;theme++) {
        const lv_image_dsc_t *const *frames=portrait_frames[theme];
        if(frames)for(unsigned n=0;n<ONLINE_AVATAR_COUNT;n++)if(source==frames[n])return frames[n];
    }
    return NULL;
}
static lv_result_t info(lv_image_decoder_t *decoder,lv_image_decoder_dsc_t *dsc,lv_image_header_t *header) {
    (void)decoder;
    const lv_image_dsc_t *frame=frame_for(dsc->src);
    if(!frame)return LV_RESULT_INVALID;
    *header=(lv_image_header_t){.magic=LV_IMAGE_HEADER_MAGIC,.cf=LV_COLOR_FORMAT_RGB565A8,
        .w=WHALE_VIEW_WIDTH,.h=frame->header.h,.stride=WHALE_VIEW_WIDTH*2};
    return LV_RESULT_OK;
}
static lv_result_t open_image(lv_image_decoder_t *decoder,lv_image_decoder_dsc_t *dsc) {
    (void)decoder;
    const lv_image_dsc_t *frame=frame_for(dsc->src);
    if(!frame || frame->header.cf!=LV_COLOR_FORMAT_RAW ||
        frame->header.w!=WHALE_VIEW_WIDTH || !frame->header.h || frame->header.h>WHALE_VIEW_HEIGHT ||
        frame->data_size<=WHALE_AVATAR_PALETTE_BYTES)return LV_RESULT_INVALID;
    dsc->user_data=(void *)frame;dsc->args.no_cache=true;dsc->decoded=NULL;
    return LV_RESULT_OK;
}
static bool decode_row(const lv_image_dsc_t *frame,unsigned y,unsigned left,unsigned width,lv_draw_buf_t *row) {
    const uint8_t *palette=frame->data;
    const uint8_t *indices=whale_band_cached_row(palette+WHALE_AVATAR_PALETTE_BYTES,
        frame->data_size-WHALE_AVATAR_PALETTE_BYTES,frame->header.w,frame->header.h,y,&whale_image_scratch.inflate);
    if(!indices)return false;
    uint8_t *color=row->data,*mask=color+row->header.stride;
    for(unsigned n=0;n<width;n++) {
        const uint8_t *entry=palette+3*indices[left+n];
        color[2*n]=entry[0];color[2*n+1]=entry[1];mask[n]=entry[2];
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
    if(!dsc->user_data || area->x1<0 || area->x2>=WHALE_VIEW_WIDTH || area->y1<0 ||
       area->y1>=((const lv_image_dsc_t *)dsc->user_data)->header.h || width<=0)return LV_RESULT_INVALID;
    /* RGB565A8 uses a two-byte color stride and a separate one-byte A8 plane. */
    lv_draw_buf_t *row=lv_draw_buf_reshape(&scanline,LV_COLOR_FORMAT_RGB565A8,(unsigned)width,1,(unsigned)width*2);
    if(!row || !decode_row(dsc->user_data,(unsigned)area->y1,(unsigned)area->x1,(unsigned)width,row))
        return LV_RESULT_INVALID;
    dsc->decoded=row;return LV_RESULT_OK;
}
static void close_image(lv_image_decoder_t *decoder,lv_image_decoder_dsc_t *dsc) {
    (void)decoder;dsc->user_data=NULL;dsc->decoded=NULL;
}
void whale_avatar_view_init(void) {
    if(registered)return;
    lv_draw_buf_init(&scanline,WHALE_VIEW_WIDTH,1,LV_COLOR_FORMAT_RGB565A8,
        WHALE_VIEW_WIDTH*2,whale_image_scratch.pixels,sizeof(whale_image_scratch.pixels));
    lv_image_decoder_t *decoder=lv_image_decoder_create();
    if(!decoder)return;
    lv_image_decoder_set_info_cb(decoder,info);lv_image_decoder_set_open_cb(decoder,open_image);
    lv_image_decoder_set_get_area_cb(decoder,get_area);lv_image_decoder_set_close_cb(decoder,close_image);
    registered=true;
}
const lv_image_dsc_t *whale_avatar_view_source(online_theme_t theme,online_avatar_t frame) {
    if(!registered || (unsigned)theme>=ONLINE_THEME_COUNT || !portrait_frames[theme])return NULL;
    const lv_image_dsc_t *const *frames=portrait_frames[theme];
    if((unsigned)frame>=ONLINE_AVATAR_COUNT || !frames[frame])frame=ONLINE_AVATAR_IDLE;
    return frames[frame];
}
