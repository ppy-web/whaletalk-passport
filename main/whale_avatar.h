#pragma once
#include "lvgl.h"
#include "online_state.h"
/* RAW payload: 256 RGB565LE/A8 palette entries, then indexed Deflate bands. */
#define WHALE_AVATAR_PALETTE_BYTES (256*3)
extern const lv_image_dsc_t *const whale_avatar_frames[ONLINE_AVATAR_COUNT];
extern const lv_image_dsc_t *const whale_maid_avatar_frames[ONLINE_AVATAR_COUNT];
extern const lv_image_dsc_t *const claude_avatar_frames[ONLINE_AVATAR_COUNT];
extern const lv_image_dsc_t *const chatgpt_avatar_frames[ONLINE_AVATAR_COUNT];
