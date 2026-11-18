#pragma once
#include "lvgl.h"
#include "online_state.h"

/* All presets share uncompressed 8-bit indexed BMPs with the setup portal.
 * Day/night are 240×320 and seasonal images are 120×160. */
#define WHALE_SEASON_WIDTH 120
#define WHALE_SEASON_HEIGHT 160
#define WHALE_SEASON_PALETTE_OFFSET 54
#define WHALE_SEASON_PIXEL_OFFSET (WHALE_SEASON_PALETTE_OFFSET+256*4)
#define WHALE_SEASON_BYTES (WHALE_SEASON_PIXEL_OFFSET+WHALE_SEASON_WIDTH*WHALE_SEASON_HEIGHT)
extern const lv_image_dsc_t *const whale_backgrounds[ONLINE_BACKGROUND_COUNT];
