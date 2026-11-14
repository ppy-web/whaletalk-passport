#pragma once
#include "lvgl.h"
#include "online_state.h"
/* RAW payload: 256 RGB565LE/A8 palette entries, then (height + 1) little
 * endian uint16 offsets into the following row data. A width-byte row is
 * literal I8; a shorter row contains packets. Bit 7 means a repeated index
 * instead of literal indices, and bits 0..6 store the pixel count minus one. */
#define WHALE_AVATAR_PALETTE_BYTES (256*3)
extern const lv_image_dsc_t *const whale_avatar_frames[ONLINE_AVATAR_COUNT];
extern const lv_image_dsc_t *const whale_maid_avatar_frames[ONLINE_AVATAR_COUNT];
extern const lv_image_dsc_t *const claude_avatar_frames[ONLINE_AVATAR_COUNT];
