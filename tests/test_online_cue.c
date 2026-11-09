#include "online_cue.h"
#include <assert.h>
#include <stdio.h>

int main(void) {
    assert(online_cue_capture_allowed(false,false,false,false));
    assert(!online_cue_capture_allowed(true,false,false,false));
    assert(!online_cue_capture_allowed(true,true,false,true));
    assert(!online_cue_capture_allowed(true,true,true,false));
    assert(online_cue_capture_allowed(true,true,true,true));
    assert(!online_cue_upload_allowed(true,false) && online_cue_upload_allowed(true,true));
    assert(online_cue_upload_allowed(false,false));
    assert(!online_cue_play_allowed(CUE_ERROR,false,50,false,false,false,false,false,false));
    assert(!online_cue_play_allowed(CUE_ERROR,true,0,false,false,false,false,false,false));
    assert(!online_cue_play_allowed(CUE_ERROR,true,50,false,false,false,false,false,true));
    assert(!online_cue_play_allowed(CUE_CANCEL,true,50,false,false,true,false,false,false));
    assert(!online_cue_play_allowed(CUE_VOLUME,true,50,false,false,false,true,false,false));
    assert(!online_cue_play_allowed(CUE_PTT_SEND,true,50,false,false,true,true,false,false));
    assert(online_cue_play_allowed(CUE_PTT_SEND,true,50,false,false,true,true,true,false));
    assert(!online_cue_play_allowed(CUE_PTT_START,true,50,true,false,false,false,false,false));
    assert(online_cue_play_allowed(CUE_PTT_START,true,50,true,true,true,true,false,false));
    online_cue_gate_t gate={0};online_cue_gate_begin(&gate,7,100000);
    for(unsigned n=0;n<6;n++) {assert(!online_cue_gate_ready(&gate,7,200000));online_cue_gate_discard(&gate);}
    assert(!online_cue_gate_ready(&gate,7,159999));
    assert(!online_cue_gate_ready(&gate,8,200000));
    assert(online_cue_gate_ready(&gate,7,160000));
    online_cue_queue_t queue={0};online_cue_t cue;
    assert(online_cue_push(&queue,CUE_MIC_ON,100));
    assert(!online_cue_push(&queue,CUE_MIC_ON,101));
    assert(online_cue_push(&queue,CUE_MIC_OFF,102));
    assert(online_cue_push(&queue,CUE_CANCEL,103));
    assert(online_cue_push(&queue,CUE_VOLUME,104));
    assert(!online_cue_push(&queue,CUE_CONNECTED,105));
    assert(online_cue_push(&queue,CUE_ERROR,106));
    assert(online_cue_pop(&queue,107,&cue) && cue==CUE_MIC_OFF);
    assert(!online_cue_pop(&queue,1000,&cue));
    assert(online_cue_push(&queue,CUE_CONNECTED,2000));online_cue_clear(&queue);
    assert(!online_cue_pop(&queue,2001,&cue));
    for(unsigned type=0;type<CUE_COUNT;type++) {
        int16_t samples[160];unsigned audible=0;
        for(unsigned block=0;block<8;block++) {
            assert(online_cue_render((online_cue_t)type,block,samples));
            for(unsigned i=0;i<160;i++) {assert(samples[i]>=-2500 && samples[i]<=2500);if(samples[i])audible++;}
        }
        assert(audible>100 && !online_cue_render((online_cue_t)type,8,samples));
    }
    puts("Cue synthesis, bounded queue, deduplication and expiry: PASS");
}
