#include "online_input.h"

bool online_input_sync(online_input_control_t *control,bool enabled,
                       online_input_send_t send,void *context) {
    if(control->synced && control->enabled==enabled)return true;
    control->synced=false;
    if(enabled && !send("wake",context))return false;
    if(!send(enabled?"input.unmute":"input.mute",context))return false;
    control->enabled=enabled;
    control->synced=true;
    return true;
}
