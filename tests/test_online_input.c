#include "online_input.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

typedef struct { const char *events[16]; unsigned count,fail_at; } transport_t;
static bool send_event(const char *type,void *context) {
    transport_t *t=context;
    assert(t->count<16);
    t->events[t->count++]=type;
    return t->count!=t->fail_at;
}
static void expect(transport_t *t,unsigned index,const char *type) {
    assert(index<t->count && !strcmp(t->events[index],type));
}
int main(void) {
    online_input_control_t input={0};transport_t t={0};
    assert(online_input_sync(&input,true,send_event,&t));
    expect(&t,0,"wake");expect(&t,1,"input.unmute");
    assert(input.synced && input.enabled);
    assert(online_input_sync(&input,true,send_event,&t) && t.count==2);
    // Standby mutes the retained session; wake restores the user's open mic.
    assert(online_input_sync(&input,false,send_event,&t));
    expect(&t,2,"input.mute");assert(!input.enabled);
    assert(online_input_sync(&input,false,send_event,&t) && t.count==3);
    assert(online_input_sync(&input,true,send_event,&t));
    expect(&t,3,"wake");expect(&t,4,"input.unmute");
    // Reconnecting in standby must explicitly establish muted input again.
    input.synced=false;t.count=0;
    assert(online_input_sync(&input,false,send_event,&t));
    expect(&t,0,"input.mute");assert(t.count==1 && !input.enabled);
    // Failed controls never authorize PCM upload. Retry repeats the ordering.
    for(unsigned fail=1;fail<=2;fail++) {
        input=(online_input_control_t){0};t=(transport_t){.fail_at=fail};
        assert(!online_input_sync(&input,true,send_event,&t));
        assert(!input.synced && t.count==fail);
        t=(transport_t){0};
        assert(online_input_sync(&input,true,send_event,&t));
        expect(&t,0,"wake");expect(&t,1,"input.unmute");
    }
    t=(transport_t){.fail_at=1};
    assert(!online_input_sync(&input,false,send_event,&t) && !input.synced);
    t=(transport_t){0};
    assert(online_input_sync(&input,false,send_event,&t));expect(&t,0,"input.mute");
    puts("Input control: standby, wake, reconnect and transport failure PASS");
}
