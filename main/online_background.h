#pragma once
#include "online_state.h"

/* Browser-quantized, fixed-size upload; no image parser runs on the device.
 * WTBG, version=1, width=96, height=128, colors=16, RGB565LE palette, I4 pixels. */
#define ONLINE_BG_WIDTH 96
#define ONLINE_BG_HEIGHT 128
#define ONLINE_BG_PIXELS (ONLINE_BG_WIDTH*ONLINE_BG_HEIGHT)
#define ONLINE_BG_BYTES (8+32+ONLINE_BG_PIXELS/2)
bool online_background_valid(const uint8_t *data,size_t size);
bool online_background_decode(const uint8_t *data,size_t size,uint16_t *out,size_t pixels);
/* Decode a clipped row directly at display resolution; no full-image scratch. */
bool online_background_decode_row(const uint8_t *data,size_t size,unsigned x,unsigned y,unsigned width,uint16_t *out);
online_background_t online_background_load(void);
bool online_background_save(online_background_t background);
bool online_background_store(const uint8_t *data,size_t size);
/* Called once by the boot worker; retain only the 6,184-byte encoded image.
 * Published data is immutable until reboot. Display decoding uses one row. */
void online_background_init(void);
const uint8_t *online_background_data(void);
bool online_background_custom_available(void);
