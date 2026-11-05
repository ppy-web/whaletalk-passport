#pragma once
#include "online_state.h"
typedef struct { online_phase_t phase; bool mic,linked,audio_ok; unsigned volume, level; char message[128]; bool configured,password_set,token_set; char ssid[33],host[64]; online_theme_t theme; bool theme_save_failed; online_idle_t idle; bool idle_save_failed; online_background_t background; bool custom_background,background_save_failed; unsigned brightness; bool brightness_save_failed; } online_status_t;
void online_start(bool audio_ok);
void online_snapshot(online_status_t *out);
/* Thread-safe, non-blocking commands; worker owns all hardware/network I/O. */
void online_toggle_mic(void);
void online_cancel(void);
void online_volume(void);
void online_setup(void);
void online_cycle_theme(void);
void online_cycle_idle(void);
void online_cycle_background(void);
void online_cycle_brightness(void);
