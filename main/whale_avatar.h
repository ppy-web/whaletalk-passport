#pragma once
#include "lvgl.h"
#include "online_state.h"
/* RAW payload: 256 interleaved RGB565LE/A8 palette entries, then I8 pixels.
 * Runtime decoding produces native RGB565A8 scanlines, never a full image. */
#define WHALE_AVATAR_PALETTE_BYTES (256*3)
extern const lv_image_dsc_t *const whale_avatar_frames[ONLINE_AVATAR_COUNT];
