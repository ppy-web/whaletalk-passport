#pragma once
#include "lvgl.h"
#include "online_state.h"

/* Presets store a BGR888 palette and indexed Deflate bands at native size.
 * Day/night are 240×320 and seasonal images are 120×160. */
extern const lv_image_dsc_t *const whale_backgrounds[ONLINE_BACKGROUND_COUNT];
