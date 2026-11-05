#include "online_background.h"
#include <string.h>

bool online_background_valid(const uint8_t *data,size_t size) {
    static const uint8_t header[]={'W','T','B','G',1,ONLINE_BG_WIDTH,ONLINE_BG_HEIGHT,16};
    return data && size==ONLINE_BG_BYTES && !memcmp(data,header,sizeof(header));
}
bool online_background_decode(const uint8_t *data,size_t size,uint16_t *out,size_t pixels) {
    if(!online_background_valid(data,size) || !out || pixels<ONLINE_BG_PIXELS)return false;
    for(size_t n=0;n<ONLINE_BG_PIXELS;n++) {
        unsigned byte=data[40+n/2],index=n%2?byte&15:byte>>4;
        out[n]=(uint16_t)data[8+index*2] | (uint16_t)data[9+index*2]<<8;
    }
    return true;
}
bool online_background_decode_row(const uint8_t *data,size_t size,unsigned x,unsigned y,unsigned width,uint16_t *out) {
    if(!online_background_valid(data,size) || !out || x>=240 || y>=320 || !width || width>240-x)return false;
    unsigned source_y=y*ONLINE_BG_HEIGHT/320;
    for(unsigned n=0;n<width;n++) {
        unsigned source_x=(x+n)*ONLINE_BG_WIDTH/240;
        unsigned pixel=source_y*ONLINE_BG_WIDTH+source_x;
        unsigned byte=data[40+pixel/2],index=pixel%2?byte&15:byte>>4;
        out[n]=(uint16_t)data[8+index*2] | (uint16_t)data[9+index*2]<<8;
    }
    return true;
}
