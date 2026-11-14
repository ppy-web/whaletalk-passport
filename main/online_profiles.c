#include "online_profiles.h"
#include "nvs.h"
#include <stdlib.h>
#include <stdatomic.h>
#include <string.h>
static atomic_flag mutating=ATOMIC_FLAG_INIT;

bool online_profiles_valid(const online_profiles_t *p) {
    if(!p || p->version!=1 || p->count>ONLINE_PROFILE_LIMIT)return false;
    for(unsigned i=0;i<p->count;i++) {
        if(!online_config_valid(&p->entries[i]))return false;
        for(unsigned j=0;j<i;j++)if(!strcmp(p->entries[i].ssid,p->entries[j].ssid))return false;
    }
    return true;
}
static unsigned profile_index(const online_profiles_t *p,const char *ssid) {
    unsigned index=0;
    while(index<p->count && strcmp(p->entries[index].ssid,ssid))index++;
    return index;
}
bool online_profiles_remember(online_profiles_t *p,const online_config_t *c) {
    if(!online_profiles_valid(p) || !online_config_valid(c))return false;
    unsigned index=profile_index(p,c->ssid);
    if(index==p->count && p->count<ONLINE_PROFILE_LIMIT)p->count++;
    if(index>=ONLINE_PROFILE_LIMIT)index=ONLINE_PROFILE_LIMIT-1;
    /* c can refer to an entry of p, including the one being moved. */
    online_config_t selected=*c;
    memmove(&p->entries[1],&p->entries[0],index*sizeof(selected));
    p->entries[0]=selected;memset(&selected,0,sizeof(selected));
    return true;
}
static bool load(online_profiles_t *p) {
    memset(p,0,sizeof(*p));p->version=1;
    nvs_handle_t n;esp_err_t e=nvs_open("dsh_voice",NVS_READONLY,&n);
    if(e==ESP_ERR_NVS_NOT_FOUND)return true;
    if(e!=ESP_OK)return false;
    size_t size=sizeof(*p);e=nvs_get_blob(n,"profiles",p,&size);
    if(e==ESP_OK) {
        nvs_close(n);return size==sizeof(*p) && online_profiles_valid(p);
    }
    if(e!=ESP_ERR_NVS_NOT_FOUND) {nvs_close(n);return false;}
    memset(p,0,sizeof(*p));p->version=1;
    size=sizeof(p->entries[0]);e=nvs_get_blob(n,"config",&p->entries[0],&size);nvs_close(n);
    if(e==ESP_ERR_NVS_NOT_FOUND || e==ESP_ERR_NVS_INVALID_LENGTH || e==ESP_ERR_NVS_TYPE_MISMATCH)return true;
    if(e!=ESP_OK)return false;
    /* An unusable legacy record did not prevent setup in older firmware. */
    if(size==sizeof(p->entries[0]) && online_config_valid(&p->entries[0]))p->count=1;
    else memset(&p->entries[0],0,sizeof(p->entries[0]));
    return true;
}
static void release(online_profiles_t *p) {if(p){memset(p,0,sizeof(*p));free(p);}}
static bool store(const online_profiles_t *p) {
    nvs_handle_t n;esp_err_t e=nvs_open("dsh_voice",NVS_READWRITE,&n);
    if(e!=ESP_OK)return false;
    e=nvs_set_blob(n,"profiles",p,sizeof(*p));
    if(e==ESP_OK)e=nvs_commit(n);
    nvs_close(n);return e==ESP_OK;
}
bool online_profile_current(online_config_t *c) {
    memset(c,0,sizeof(*c));
    online_profiles_t *p=malloc(sizeof(*p));
    bool ok=p && load(p) && p->count;
    if(ok)*c=p->entries[0];
    release(p);return ok;
}
bool online_profiles_summary(char ssids[ONLINE_PROFILE_LIMIT][33],unsigned *count) {
    memset(ssids,0,ONLINE_PROFILE_LIMIT*33);*count=0;
    online_profiles_t *p=malloc(sizeof(*p));bool ok=p && load(p);
    if(ok) {
        *count=p->count;
        for(unsigned i=0;i<p->count;i++)memcpy(ssids[i],p->entries[i].ssid,33);
    }
    release(p);return ok;
}
bool online_profile_save(const online_config_t *c) {
    if(!online_config_valid(c))return false;
    if(atomic_flag_test_and_set(&mutating))return false;
    online_profiles_t *p=malloc(sizeof(*p));
    bool ok=p && load(p) && online_profiles_remember(p,c) && store(p);
    release(p);atomic_flag_clear(&mutating);return ok;
}
bool online_profile_select(const char *ssid) {
    if(!ssid || !*ssid)return false;
    if(atomic_flag_test_and_set(&mutating))return false;
    online_profiles_t *p=malloc(sizeof(*p));
    bool ok=p && load(p);
    if(ok) {
        unsigned index=profile_index(p,ssid);
        ok=index<p->count && (!index || (online_profiles_remember(p,&p->entries[index]) && store(p)));
    }
    release(p);atomic_flag_clear(&mutating);return ok;
}
