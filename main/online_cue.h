#pragma once
#include <stdbool.h>
#include <stdint.h>

typedef enum { CUE_MIC_ON, CUE_MIC_OFF, CUE_PTT_START, CUE_PTT_SEND, CUE_CANCEL,
    CUE_CONNECTED, CUE_DISCONNECTED, CUE_ERROR, CUE_VOLUME, CUE_COUNT } online_cue_t;
typedef struct { online_cue_t kind; uint64_t at; } online_cue_event_t;
typedef struct { online_cue_event_t events[4]; unsigned count; } online_cue_queue_t;
/* Caller serializes queue access. Duplicate entries coalesce; errors can replace old cues. */
bool online_cue_push(online_cue_queue_t *queue,online_cue_t kind,uint64_t now);
bool online_cue_pop(online_cue_queue_t *queue,uint64_t now,online_cue_t *kind);
void online_cue_clear(online_cue_queue_t *queue);
/* Eight 10 ms PCM16 blocks at 16 kHz, generated without allocation. */
bool online_cue_render(online_cue_t kind,unsigned block,int16_t samples[160]);
bool online_cue_capture_allowed(bool wait_cue,bool active,bool cue_played,bool tail_elapsed);
bool online_cue_upload_allowed(bool wait_cue,bool audio_started);
bool online_cue_play_allowed(online_cue_t kind,bool enabled,unsigned volume,bool holding,bool active,bool busy,bool capturing,bool manual_pending,bool playback_pending);
typedef struct { unsigned sequence,drained; uint64_t ready_at; } online_cue_gate_t;
void online_cue_gate_begin(online_cue_gate_t *gate,unsigned sequence,uint64_t now_us);
void online_cue_gate_discard(online_cue_gate_t *gate);
bool online_cue_gate_ready(const online_cue_gate_t *gate,unsigned sequence,uint64_t now_us);
