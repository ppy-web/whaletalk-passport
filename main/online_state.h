#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdatomic.h>
typedef enum { ONLINE_SETUP, ONLINE_CONNECTING, ONLINE_READY, ONLINE_LISTENING,
    ONLINE_THINKING, ONLINE_SPEAKING, ONLINE_ERROR } online_phase_t;
/* Stored as dsh_voice/theme; keep the original values when adding options. */
typedef enum {
    ONLINE_THEME_WHALE=0, ONLINE_THEME_MINT=1, ONLINE_THEME_WHALE_MAID=2, ONLINE_THEME_CLAUDE=3,
    ONLINE_THEME_COUNT
} online_theme_t;
online_theme_t online_theme_normalize(unsigned stored);
online_theme_t online_theme_next(online_theme_t theme);
/* Persisted in dsh_voice/background and used by the local settings portal. */
typedef enum {
    ONLINE_BACKGROUND_DAY=0, ONLINE_BACKGROUND_NIGHT=1, ONLINE_BACKGROUND_CUSTOM=2, ONLINE_BACKGROUND_OFF=3,
    ONLINE_BACKGROUND_SPRING=4, ONLINE_BACKGROUND_SUMMER=5, ONLINE_BACKGROUND_AUTUMN=6, ONLINE_BACKGROUND_WINTER=7,
    ONLINE_BACKGROUND_COUNT
} online_background_t;
online_background_t online_background_normalize(unsigned stored);
online_background_t online_background_next(online_background_t current,bool has_custom);
typedef enum { ONLINE_AVATAR_IDLE, ONLINE_AVATAR_LISTENING, ONLINE_AVATAR_THINKING,
    ONLINE_AVATAR_SPEAKING, ONLINE_AVATAR_BLINK, ONLINE_AVATAR_ERROR, ONLINE_AVATAR_COUNT } online_avatar_t;
online_avatar_t online_avatar_frame(online_phase_t phase,bool mic,unsigned level,uint64_t now_ms);
unsigned online_volume_next(unsigned current);
unsigned online_volume_normalize(unsigned stored);
#define ONLINE_VOLUME_DEFAULT 75
unsigned online_brightness_normalize(unsigned stored);
unsigned online_brightness_next(unsigned current);

/* Idle timeout for connected standby; microphone choice survives wake. */
typedef enum { ONLINE_IDLE_1_MIN, ONLINE_IDLE_3_MIN, ONLINE_IDLE_10_MIN, ONLINE_IDLE_NEVER } online_idle_t;
online_idle_t online_idle_normalize(unsigned stored);
online_idle_t online_idle_next(online_idle_t current);
uint64_t online_idle_timeout_ms(online_idle_t setting);
typedef struct { bool initialized, asleep; uint64_t activity_ms; } online_screen_t;
bool online_screen_step(online_screen_t *screen,uint64_t now_ms,bool activity,bool keep_awake,uint64_t timeout_ms);
/* Configuration is stored as one versioned NVS blob, never in the image. */
typedef struct { uint32_t version; char ssid[33], password[65], url[256], token[193]; } online_config_t;
bool online_config_valid(const online_config_t *c);
bool online_reuse_wifi(online_config_t *next,const online_config_t *previous);
bool online_fragment(size_t *used, size_t capacity, size_t offset, size_t length, size_t total);
enum { PTT_IDLE, PTT_ARMED, PTT_ACTIVE, PTT_DRAINING, PTT_FAILED };
bool online_standby_busy(online_phase_t phase,unsigned ptt_mode,bool reply_pending,
                         bool host_busy,bool capturing,bool feedback_pending);
typedef struct {
    atomic_uint mode,sequence,ack,problem;
    atomic_bool supported,reply,capture_stopped,audio_started;
} online_ptt_control_t;
void online_ptt_arm_state(online_ptt_control_t *ptt);
void online_ptt_start_state(online_ptt_control_t *ptt,bool ready);
void online_ptt_release_state(online_ptt_control_t *ptt);
bool online_ptt_release_waiting(online_ptt_control_t *ptt,bool wait_cue);
void online_ptt_fail_state(online_ptt_control_t *ptt,unsigned problem);
bool online_ptt_frame_valid(const online_ptt_control_t *ptt,unsigned sequence,bool sending);
bool online_ptt_commit_ready(const online_ptt_control_t *ptt,unsigned sequence,bool queue_empty);
const char *online_phase_text(online_phase_t phase);
bool online_session_compatible(const char *version, bool input_audio, bool receipts);
const char *online_error_text(const char *code);
bool online_error_recoverable(const char *code);
typedef struct { char turn[64]; const char *phase; const char *problem; bool settled,waiting; uint64_t since; } online_feedback_t;
bool online_feedback_progress(online_feedback_t *f,const char *turn,const char *phase,uint64_t now);
bool online_feedback_result(online_feedback_t *f,const char *turn,const char *result,const char *code);
void online_feedback_text(const online_feedback_t *f,uint64_t now,char *out,size_t size);
const char *online_connection_error(int http_status,int socket_error,bool has_ip);

bool online_endpoint_from_ip(const char *ip,char *out,size_t size);
bool online_endpoint_from_ip_port(const char *ip,unsigned port,char *out,size_t size);

typedef enum { ONLINE_HOME, ONLINE_SETTINGS, ONLINE_DISPLAY, ONLINE_STATUS, ONLINE_SOUND, ONLINE_CONNECTION } online_page_t;
typedef enum {
    ONLINE_KEY_OK=1, ONLINE_KEY_UP, ONLINE_KEY_DOWN, ONLINE_KEY_LONG,
    ONLINE_KEY_UP_LONG, ONLINE_KEY_DOWN_LONG
} online_key_t;
typedef enum { ONLINE_ACTION_NONE, ONLINE_ACTION_MIC, ONLINE_ACTION_VOLUME,
    ONLINE_ACTION_CANCEL, ONLINE_ACTION_SETUP, ONLINE_ACTION_THEME, ONLINE_ACTION_IDLE,
    ONLINE_ACTION_BACKGROUND, ONLINE_ACTION_BRIGHTNESS, ONLINE_ACTION_OVERLAY,
    ONLINE_ACTION_CUE, ONLINE_ACTION_NETWORK, ONLINE_ACTION_RESUME } online_action_t;
typedef enum {
    ONLINE_MENU_DISPLAY, ONLINE_MENU_STATUS, ONLINE_MENU_SOUND,
    ONLINE_MENU_UPLOAD, ONLINE_MENU_HOME, ONLINE_MENU_CONNECTION, ONLINE_MENU_COUNT
} online_menu_item_t;
enum { ONLINE_MENU_COLUMNS=2 };
typedef enum { ONLINE_DISPLAY_THEME, ONLINE_DISPLAY_BACKGROUND, ONLINE_DISPLAY_BRIGHTNESS, ONLINE_DISPLAY_IDLE,
    ONLINE_DISPLAY_OVERLAY, ONLINE_DISPLAY_BACK, ONLINE_DISPLAY_COUNT } online_display_item_t;
typedef enum { ONLINE_SOUND_VOLUME, ONLINE_SOUND_CUE, ONLINE_SOUND_BACK, ONLINE_SOUND_COUNT } online_sound_item_t;
typedef struct {
    online_page_t page;
    unsigned selected,network_count;
    bool provisioning;
} online_menu_t;
unsigned online_menu_items(const online_menu_t *menu);
online_action_t online_menu_key(online_menu_t *menu,online_key_t key);

/* Start once roughly 300 ms is queued, or after a bounded short-answer wait. */
bool online_playback_prefill_wait(unsigned queued,unsigned elapsed_ms);

/* Monotonic timing: warn and switch time sources every ten seconds. */
typedef struct { bool active, timed_out; unsigned source_index; uint64_t started_ms, retry_ms; } online_time_wait_t;
typedef enum { ONLINE_TIME_IDLE, ONLINE_TIME_START, ONLINE_TIME_WAIT, ONLINE_TIME_RETRY } online_time_action_t;
online_time_action_t online_time_step(online_time_wait_t *wait, bool has_ip,
                                     bool needs_time, uint64_t now_ms);
