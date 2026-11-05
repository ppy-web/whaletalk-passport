#pragma once
#include "lvgl.h"
/* LVGL-task only. One immutable custom source and one 480-byte scanline. */
void online_background_image_init(void);
const lv_image_dsc_t *online_background_image(const uint8_t *encoded);
