#pragma once
#include "lvgl.h"
#include "whale_band.h"

/* A draw unit consumes a scanline before asking either decoder for another.
 * Inflation and RGB565/A8 expansion can reuse the same aligned storage. */
typedef union {
    whale_band_workspace_t inflate;
    uint8_t pixels[WHALE_BAND_MAX_WIDTH * 3 + LV_DRAW_BUF_ALIGN];
} whale_image_scratch_t;

extern whale_image_scratch_t whale_image_scratch;
