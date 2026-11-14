#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define ONLINE_SETUP_URL "http://192.168.4.1"
void online_hotspot_name(const uint8_t mac[6],char ssid[33]);
bool online_hotspot_qr(const char *ssid,char *out,size_t capacity);
