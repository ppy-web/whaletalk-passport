#pragma once
#include "online_state.h"
online_theme_t online_theme_load(void);
bool online_theme_save(online_theme_t theme);
online_idle_t online_idle_load(void);
bool online_idle_save(online_idle_t setting);
