#pragma once
#include "online_state.h"
#include "online_profiles.h"
typedef struct {
    online_phase_t phase;
    bool mic,linked,has_ip,audio_ok,feedback_visible,ptt_recording,keep_awake;
    unsigned volume,level;
    char message[128];
    const char *progress; /* Immutable local progress label, or NULL. */
    bool configured,token_set;
    char ssid[33],host[64];
    unsigned profile_count;
    char profile_ssids[ONLINE_PROFILE_LIMIT][33];
    bool profile_save_failed;
    online_theme_t theme; bool theme_save_failed;
    online_idle_t idle; bool idle_save_failed;
    online_background_t background; bool custom_background,background_save_failed;
    unsigned brightness; bool brightness_save_failed;
    bool overlay,overlay_save_failed,cues,cues_save_failed,volume_save_failed;
} online_status_t;
void online_start(bool audio_ok);
void online_snapshot(online_status_t *out);
/* Follows screen idle; keeps the user's microphone preference intact. */
bool online_set_standby(bool asleep);
/* Thread-safe, non-blocking commands; worker owns all hardware/network I/O. */
void online_toggle_mic(void);
void online_cancel(void);
void online_volume(void);
void online_setup(void);
void online_select_profile(unsigned index);
void online_cycle_theme(void);
void online_cycle_idle(void);
void online_cycle_background(void);
void online_cycle_brightness(void);
void online_toggle_overlay(void);
void online_toggle_cues(void);
/* Button callbacks may call these; audio and network workers own the I/O. */
void online_ptt_press(void);
void online_ptt_start(void);
void online_ptt_release(void);
void online_ptt_abort(void);
