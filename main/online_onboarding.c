#include "online_onboarding.h"
#include <stdio.h>
#include <string.h>

void online_hotspot_name(const uint8_t mac[6],char ssid[33]) {
    snprintf(ssid,33,"DSH-Voice-%02X%02X%02X",mac[3],mac[4],mac[5]);
}
static bool valid_ssid(const char *ssid) {return ssid && *ssid && strlen(ssid)<=32;}
bool online_hotspot_qr(const char *ssid,char *out,size_t cap) {
    if(!out || !cap)return false;
    out[0]=0;if(!valid_ssid(ssid))return false;
    char payload[84]="WIFI:T:nopass;S:";size_t used=strlen(payload);
    for(const char *s=ssid;*s;s++) {
        if(strchr("\\;,:\"",*s))payload[used++]='\\';
        payload[used++]=*s;
    }
    payload[used++]=';';payload[used++]=';';payload[used]=0;
    if(used>=cap)return false;
    memcpy(out,payload,used+1);return true;
}
