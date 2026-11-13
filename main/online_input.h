#pragma once
#include <stdbool.h>

/* A successful control exchange fences PCM upload. Reset on every hello. */
typedef struct { bool synced,enabled; } online_input_control_t;
typedef bool (*online_input_send_t)(const char *type,void *context);
bool online_input_sync(online_input_control_t *control,bool enabled,
                       online_input_send_t send,void *context);
