#include "online_cue.h"
#include <string.h>

bool online_cue_capture_allowed(bool wait_cue,bool active,bool cue_played,bool tail_elapsed) {
    return !wait_cue || (active && cue_played && tail_elapsed);
}
bool online_cue_upload_allowed(bool wait_cue,bool audio_started) {return !wait_cue || audio_started;}
bool online_cue_play_allowed(online_cue_t kind,bool enabled,unsigned volume,bool holding,bool active,bool busy,bool capturing,bool manual_pending,bool playback_pending) {
    if(!enabled || !volume || (unsigned)kind>=CUE_COUNT)return false;
    if(holding)return active;
    return !active && ((!busy && !capturing) || (kind==CUE_PTT_SEND && manual_pending)) && !playback_pending;
}
void online_cue_gate_begin(online_cue_gate_t *gate,unsigned sequence,uint64_t now_us) {
    gate->sequence=sequence;gate->drained=0;gate->ready_at=now_us+60000;
}
void online_cue_gate_discard(online_cue_gate_t *gate) {if(gate->drained<6)gate->drained++;}
bool online_cue_gate_ready(const online_cue_gate_t *gate,unsigned sequence,uint64_t now_us) {
    return gate->sequence==sequence && gate->drained>=6 && now_us>=gate->ready_at;
}

void online_cue_clear(online_cue_queue_t *q) {q->count=0;}
bool online_cue_push(online_cue_queue_t *q,online_cue_t kind,uint64_t now) {
    if((unsigned)kind>=CUE_COUNT)return false;
    for(unsigned i=0;i<q->count;i++)if(q->events[i].kind==kind)return false;
    if(q->count==4) {
        if(kind!=CUE_ERROR)return false;
        memmove(q->events,q->events+1,3*sizeof(q->events[0]));q->count--;
    }
    q->events[q->count++]=(online_cue_event_t){kind,now};return true;
}
bool online_cue_pop(online_cue_queue_t *q,uint64_t now,online_cue_t *kind) {
    while(q->count) {
        online_cue_event_t event=q->events[0];
        memmove(q->events,q->events+1,(--q->count)*sizeof(q->events[0]));
        if(now>=event.at && now-event.at<=800) {*kind=event.kind;return true;}
    }
    return false;
}
bool online_cue_render(online_cue_t kind,unsigned block,int16_t samples[160]) {
    static const unsigned frequencies[CUE_COUNT][2]={
        {750,1050},{1050,750},{900,1100},{1100,900},{650,500},
        {800,1200},{1000,600},{440,440},{850,850}
    };
    if((unsigned)kind>=CUE_COUNT || block>=8)return false;
    for(unsigned i=0;i<160;i++) {
        unsigned t=block*160+i,local=t%640,freq=frequencies[kind][t/640];
        unsigned phase=(local*freq)%16000;
        int triangle=phase<8000?(int)phase-4000:12000-(int)phase;
        unsigned env=local<80?local:local>=560?639-local:80;
        /* The two low error notes have a short gap. */
        if(kind==CUE_ERROR && (local>=480))env=0;
        samples[i]=(int16_t)(triangle*(int)env/128);
    }
    return true;
}
