#include "online_profiles.h"
#include "nvs.h"
#include <stddef.h>
#include <stdlib.h>
#include <stdatomic.h>
#include <string.h>

enum { LEGACY_PROFILE_LIMIT=3, PROFILE_VERSION=2 };
/* One NVS blob; entry zero is selected and most recently saved/used. */
typedef struct {
    uint32_t version,count;
    online_config_t entries[ONLINE_PROFILE_LIMIT];
} online_profiles_t;
static atomic_flag mutating=ATOMIC_FLAG_INIT;

static bool profiles_valid(const online_profiles_t *p,unsigned limit) {
    if(p->count>limit)return false;
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
static void remember(online_profiles_t *p,const online_config_t *c) {
    unsigned index=profile_index(p,c->ssid);
    if(index==p->count && p->count<ONLINE_PROFILE_LIMIT)p->count++;
    if(index>=ONLINE_PROFILE_LIMIT)index=ONLINE_PROFILE_LIMIT-1;
    /* c can refer to an entry of p, including the one being moved. */
    online_config_t selected=*c;
    memmove(&p->entries[1],&p->entries[0],index*sizeof(selected));
    p->entries[0]=selected;memset(&selected,0,sizeof(selected));
}
static bool load(online_profiles_t *p) {
    memset(p,0,sizeof(*p));p->version=PROFILE_VERSION;
    nvs_handle_t n;esp_err_t e=nvs_open("dsh_voice",NVS_READONLY,&n);
    if(e==ESP_ERR_NVS_NOT_FOUND)return true;
    if(e!=ESP_OK)return false;
    size_t size=sizeof(*p);e=nvs_get_blob(n,"profiles",p,&size);
    if(e==ESP_OK) {
        nvs_close(n);
        if(size==sizeof(*p) && p->version==PROFILE_VERSION)return profiles_valid(p,ONLINE_PROFILE_LIMIT);
        const size_t legacy_size=offsetof(online_profiles_t,entries)+LEGACY_PROFILE_LIMIT*sizeof(p->entries[0]);
        if(size!=legacy_size || p->version!=1 || !profiles_valid(p,LEGACY_PROFILE_LIMIT))return false;
        p->version=PROFILE_VERSION;
        return true;
    }
    if(e!=ESP_ERR_NVS_NOT_FOUND) {nvs_close(n);return false;}
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
bool online_profile_save(const online_config_t *c,bool reuse_wifi) {
    if(!online_config_valid(c))return false;
    if(atomic_flag_test_and_set(&mutating))return false;
    online_profiles_t *p=malloc(sizeof(*p));
    bool ok=p && load(p);
    /* The portal may have obtained these credentials before a device-side change. */
    if(ok && reuse_wifi)ok=p->count && !strcmp(p->entries[0].ssid,c->ssid) &&
        !strcmp(p->entries[0].password,c->password);
    if(ok) {
        remember(p,c);
        ok=store(p);
    }
    release(p);atomic_flag_clear(&mutating);return ok;
}
bool online_profile_select(const char *ssid) {
    if(!ssid || !*ssid)return false;
    if(atomic_flag_test_and_set(&mutating))return false;
    online_profiles_t *p=malloc(sizeof(*p));
    bool ok=p && load(p);
    if(ok) {
        unsigned index=profile_index(p,ssid);
        ok=index<p->count;
        if(ok && index) {
            remember(p,&p->entries[index]);
            ok=store(p);
        }
    }
    release(p);atomic_flag_clear(&mutating);return ok;
}
bool online_profile_delete(const char *ssid,bool *was_current) {
    *was_current=false;
    if(!ssid || !*ssid || atomic_flag_test_and_set(&mutating))return false;
    online_profiles_t *p=malloc(sizeof(*p));
    bool ok=p && load(p);
    if(ok) {
        unsigned index=profile_index(p,ssid);
        ok=index<p->count;
        if(ok) {
            p->count--;
            if(index<p->count)memmove(&p->entries[index],&p->entries[index+1],
                                      (p->count-index)*sizeof(p->entries[0]));
            memset(&p->entries[p->count],0,sizeof(p->entries[0]));
            /* Keep an empty blob so deleting the last profile suppresses config fallback. */
            ok=store(p);
            *was_current=ok && !index;
        }
    }
    release(p);atomic_flag_clear(&mutating);return ok;
}
