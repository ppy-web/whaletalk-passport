#pragma once
#include "whale_avatar.h"

/* Portraits use a native viewport below the 35px status bar.
 * Scanline decoding avoids a scaled full-frame image allocation. */
#define WHALE_VIEW_WIDTH 240
#define WHALE_VIEW_HEIGHT 285
#define WHALE_VIEW_TOP 35
void whale_avatar_view_init(void);
const lv_image_dsc_t *whale_avatar_view_source(online_theme_t theme,online_avatar_t frame);
