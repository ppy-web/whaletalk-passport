#pragma once
#include "whale_avatar.h"

/* Display composition: a centered 280px portrait at y=38, below the 35px
 * status bar so the forelock stays visible; the fade continues behind keys.
 * Native scanline decoding avoids a scaled full-frame image allocation. */
#define WHALE_VIEW_WIDTH 240
#define WHALE_VIEW_HEIGHT 285
#define WHALE_VIEW_TOP 35
void whale_avatar_view_init(void);
const lv_image_dsc_t *whale_avatar_view_source(online_theme_t theme,online_avatar_t frame);
