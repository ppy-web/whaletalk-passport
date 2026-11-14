#pragma once
#include "online_state.h"

#define ONLINE_PROFILE_LIMIT 3
/* One atomic NVS blob; entry zero is selected and most recently saved/used.
 * The original version-one config remains a read-only migration fallback. */
typedef struct {
    uint32_t version,count;
    online_config_t entries[ONLINE_PROFILE_LIMIT];
} online_profiles_t;

bool online_profiles_valid(const online_profiles_t *profiles);
bool online_profiles_remember(online_profiles_t *profiles,const online_config_t *config);
bool online_profile_current(online_config_t *config);
bool online_profiles_summary(char ssids[ONLINE_PROFILE_LIMIT][33],unsigned *count);
bool online_profile_save(const online_config_t *config);
bool online_profile_select(const char *ssid);
