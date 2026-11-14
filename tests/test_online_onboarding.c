#include "online_onboarding.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void) {
    uint8_t mac[]={0x02,0,1,0xab,0x01,0xef};char name[33],same[33],payload[84];
    online_hotspot_name(mac,name);online_hotspot_name(mac,same);
    assert(!strcmp(name,"DSH-Voice-AB01EF") && !strcmp(name,same));
    mac[5]++;online_hotspot_name(mac,same);assert(strcmp(name,same));
    assert(online_hotspot_qr(name,payload,sizeof(payload)));
    assert(!strcmp(payload,"WIFI:T:nopass;S:DSH-Voice-AB01EF;;"));
    assert(online_hotspot_qr("test;,:\\\"",payload,sizeof(payload)));
    assert(!strcmp(payload,"WIFI:T:nopass;S:test\\;\\,\\:\\\\\\\";;"));
    char longest[33];memset(longest,';',32);longest[32]=0;
    assert(online_hotspot_qr(longest,payload,sizeof(payload)));
    size_t len=strlen(payload);assert(len<sizeof(payload));
    assert(online_hotspot_qr(longest,payload,len+1));
    assert(!online_hotspot_qr(longest,payload,len) && !payload[0]);
    char too_long[34];memset(too_long,'x',33);too_long[33]=0;
    assert(!online_hotspot_qr(too_long,payload,sizeof(payload)));
    assert(!online_hotspot_qr("",payload,sizeof(payload)));
    assert(!online_hotspot_qr(NULL,payload,sizeof(payload)));
    assert(!online_hotspot_qr(name,NULL,84));assert(!online_hotspot_qr(name,payload,0));
    puts("Onboarding payloads: PASS");return 0;
}
