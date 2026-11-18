#pragma once
#include "online_state.h"

#define ONLINE_PROFILE_LIMIT 6

bool online_profile_current(online_config_t *config);
bool online_profiles_summary(char ssids[ONLINE_PROFILE_LIMIT][33],unsigned *count);
bool online_profile_save(const online_config_t *config,bool reuse_wifi);
bool online_profile_select(const char *ssid);
bool online_profile_delete(const char *ssid,bool *was_current);
