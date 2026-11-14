#include "whale_avatar_view.h"
#include "src/draw/lv_image_decoder_private.h"

static bool registered;
/* Single software draw unit, same synchronous get-area contract as custom
 * backgrounds. RGB565 and A8 planes share one native, sub-kilobyte row. */
static LV_ATTRIBUTE_MEM_ALIGN uint8_t scanline_bytes[WHALE_VIEW_WIDTH*3+LV_DRAW_BUF_ALIGN];
static lv_draw_buf_t scanline;
static const lv_image_dsc_t *const *const portrait_frames[ONLINE_THEME_COUNT]={
    [ONLINE_THEME_WHALE]=whale_avatar_frames,
    [ONLINE_THEME_WHALE_MAID]=whale_maid_avatar_frames,
    [ONLINE_THEME_CLAUDE]=claude_avatar_frames,
};
#define PIXEL_DATA_OFFSET (WHALE_AVATAR_PALETTE_BYTES+(WHALE_VIEW_HEIGHT+1)*2)
static const lv_image_dsc_t *frame_for(const void *source) {
    for(unsigned theme=0;theme<ONLINE_THEME_COUNT;theme++) {
        const lv_image_dsc_t *const *frames=portrait_frames[theme];
        if(frames)for(unsigned n=0;n<ONLINE_AVATAR_COUNT;n++)if(source==frames[n])return frames[n];
    }
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
        frame->data_size<=PIXEL_DATA_OFFSET)return LV_RESULT_INVALID;
    dsc->user_data=(void *)frame;dsc->args.no_cache=true;dsc->decoded=NULL;
    return LV_RESULT_OK;
}
static unsigned read_le16(const uint8_t *p) {
    return (unsigned)p[0]|((unsigned)p[1]<<8);
}
static void write_pixel(uint8_t *color,uint8_t *mask,const uint8_t *palette,unsigned index) {
    const uint8_t *entry=palette+index*3;
    color[0]=entry[0];color[1]=entry[1];*mask=entry[2];
}
static bool decode_row(const lv_image_dsc_t *frame,unsigned y,unsigned left,unsigned width,lv_draw_buf_t *row) {
    const uint8_t *palette=frame->data;
    const uint8_t *offset=palette+WHALE_AVATAR_PALETTE_BYTES+y*2;
    unsigned start=read_le16(offset),end=read_le16(offset+2);
    if(end<=start || end>frame->data_size-PIXEL_DATA_OFFSET || end-start>WHALE_VIEW_WIDTH)return false;
    const uint8_t *p=palette+PIXEL_DATA_OFFSET+start,*limit=p+(end-start);
    uint8_t *color=row->data,*mask=color+row->header.stride;
    if(end-start==WHALE_VIEW_WIDTH) {
        for(unsigned n=0;n<width;n++)write_pixel(color+n*2,mask+n,palette,p[left+n]);
        return true;
    }
    unsigned x=0,right=left+width;
    while(x<right && p<limit) {
        unsigned command=*p++,count=(command&127)+1;
        bool repeated=(command&128)!=0;
        unsigned bytes=repeated?1:count;
        if(x+count>WHALE_VIEW_WIDTH || (unsigned)(limit-p)<bytes)return false;
        unsigned lo=x>left?x:left,hi=x+count<right?x+count:right;
        for(unsigned i=lo;i<hi;i++)
            write_pixel(color+(i-left)*2,mask+(i-left),palette,p[repeated?0:i-x]);
        x+=count;p+=bytes;
    }
    return x>=right;
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
        WHALE_VIEW_WIDTH*2,scanline_bytes,sizeof(scanline_bytes));
    lv_image_decoder_t *decoder=lv_image_decoder_create();
    if(!decoder)return;
    lv_image_decoder_set_info_cb(decoder,info);lv_image_decoder_set_open_cb(decoder,open_image);
    lv_image_decoder_set_get_area_cb(decoder,get_area);lv_image_decoder_set_close_cb(decoder,close_image);
    registered=true;
}
const lv_image_dsc_t *whale_avatar_view_source(online_theme_t theme,online_avatar_t frame) {
    if(!registered || (unsigned)theme>=ONLINE_THEME_COUNT || !portrait_frames[theme])return NULL;
    if((unsigned)frame>=ONLINE_AVATAR_COUNT)frame=ONLINE_AVATAR_IDLE;
    return portrait_frames[theme][frame];
}
