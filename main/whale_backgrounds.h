#pragma once
#include "lvgl.h"
#include "online_state.h"

/* Each seasonal preset is a 120×160 uncompressed, 8-bit indexed BMP.
 * Both the screen and the settings portal use the same flash data. */
#define WHALE_SEASON_WIDTH 120
#define WHALE_SEASON_HEIGHT 160
#define WHALE_SEASON_PALETTE_OFFSET 54
#define WHALE_SEASON_PIXEL_OFFSET (WHALE_SEASON_PALETTE_OFFSET+256*4)
#define WHALE_SEASON_BYTES (WHALE_SEASON_PIXEL_OFFSET+WHALE_SEASON_WIDTH*WHALE_SEASON_HEIGHT)
extern const lv_image_dsc_t *const whale_backgrounds[ONLINE_BACKGROUND_COUNT];
