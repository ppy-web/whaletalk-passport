#pragma once
#include "lvgl.h"
/* LVGL task only. Custom images and seasonal presets share a 480-byte scanline. */
void online_background_image_init(void);
const lv_image_dsc_t *online_background_image(const uint8_t *encoded);
