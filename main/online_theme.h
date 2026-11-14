#pragma once
#include "online_state.h"
online_theme_t online_theme_load(void);
bool online_theme_save(online_theme_t theme);
online_idle_t online_idle_load(void);
bool online_idle_save(online_idle_t setting);
unsigned online_brightness_load(void);
bool online_brightness_save(unsigned percent);
bool online_overlay_load(void);
bool online_overlay_save(bool enabled);
bool online_cues_load(void);
bool online_cues_save(bool enabled);
unsigned online_volume_load(void);
bool online_volume_save(unsigned percent);
