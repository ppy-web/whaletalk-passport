#include "online_profiles.h"
#include "nvs.h"
#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    uint32_t version,count;
    online_config_t entries[ONLINE_PROFILE_LIMIT];
} profile_blob_t;
static profile_blob_t stored,pending;
static online_config_t legacy;
static bool has_profiles,has_legacy,has_pending,fail_open,fail_read,fail_set,fail_commit,reenter;
static unsigned handles,writes;
static size_t stored_size=sizeof(stored);
esp_err_t nvs_open(const char *name,nvs_open_mode_t mode,nvs_handle_t *handle) {
    (void)mode;assert(!strcmp(name,"dsh_voice"));
    if(fail_open)return ESP_FAIL;
    assert(!handles);*handle=1;handles++;return ESP_OK;
}
esp_err_t nvs_get_blob(nvs_handle_t handle,const char *key,void *out,size_t *size) {
    assert(handle && handles);
    if(fail_read)return ESP_FAIL;
    const void *data=NULL;size_t len=0;
    if(!strcmp(key,"profiles") && has_profiles) {data=&stored;len=stored_size;}
    else if(!strcmp(key,"config") && has_legacy) {data=&legacy;len=sizeof(legacy);}
    if(!data)return ESP_ERR_NVS_NOT_FOUND;
    if(*size<len) {*size=len;return ESP_ERR_NVS_INVALID_LENGTH;}
    memcpy(out,data,len);*size=len;return ESP_OK;
}
esp_err_t nvs_set_blob(nvs_handle_t handle,const char *key,const void *data,size_t size) {
    assert(handle && handles && !strcmp(key,"profiles") && size==sizeof(pending));
    if(reenter) {
        bool was_current;
        assert(!online_profile_select("Office"));
        assert(!online_profile_delete("Office",&was_current));
    }
    if(fail_set)return ESP_FAIL;
    memcpy(&pending,data,size);has_pending=true;return ESP_OK;
}
esp_err_t nvs_commit(nvs_handle_t handle) {
    assert(handle && handles && has_pending);
    if(fail_commit)return ESP_FAIL;
    stored=pending;has_profiles=true;stored_size=sizeof(stored);writes++;return ESP_OK;
}
void nvs_close(nvs_handle_t handle) {assert(handle && handles==1);handles--;has_pending=false;}
static online_config_t configuration(const char *ssid,unsigned host) {
    online_config_t c={.version=1};
    snprintf(c.ssid,sizeof(c.ssid),"%s",ssid);
    snprintf(c.password,sizeof(c.password),"password-%u",host);
    snprintf(c.url,sizeof(c.url),"ws://192.168.1.%u:3101/api/realtime",host);
    snprintf(c.token,sizeof(c.token),"%04u",host);
    assert(online_config_valid(&c));return c;
}
static void check(unsigned expected_count,const char *const expected[]) {
    char names[ONLINE_PROFILE_LIMIT][33];unsigned count=99;
    assert(online_profiles_summary(names,&count));
    assert(count==expected_count);
    for(unsigned i=0;i<count;i++)assert(!strcmp(names[i],expected[i]));
    assert(!handles);
}
int main(void) {
    check(0,NULL);
    online_config_t active;
    bool was_current;
    assert(!online_profile_current(&active) && !active.ssid[0]);
    assert(!online_profile_select("Home") && !online_profile_delete("Home",&was_current));
    legacy=configuration("Home",10);has_legacy=true;
    check(1,(const char *[]) {"Home"});assert(!has_profiles && !writes);
    assert(online_profile_delete("Home",&was_current) && was_current);
    check(0,NULL);
    assert(!online_profile_current(&active) && !online_profile_save(&legacy,true));
    assert(!strcmp(legacy.ssid,"Home"));

    online_config_t office=configuration("Office",20),phone=configuration("Phone",30);
    stored=(profile_blob_t){.version=1,.count=3,.entries={office,legacy,phone}};
    const size_t legacy_size=offsetof(profile_blob_t,entries)+3*sizeof(online_config_t);
    stored_size=legacy_size;
    check(3,(const char *[]) {"Office","Home","Phone"});
    assert(online_profile_current(&active) && !memcmp(&active,&office,sizeof(active)));
    assert(online_profile_select("Office") && stored_size==legacy_size);
    stored.count=4;assert(!online_profile_save(&office,false));stored.count=3;
    assert(online_profile_select("Home") && stored_size==sizeof(stored));
    check(3,(const char *[]) {"Home","Office","Phone"});

    online_config_t guest=configuration("Guest",40),travel=configuration("Travel",50);
    online_config_t cafe=configuration("Cafe",60),lab=configuration("Lab",70);
    assert(online_profile_save(&guest,false));
    assert(online_profile_save(&travel,false));
    assert(online_profile_save(&cafe,false));
    check(6,(const char *[]) {"Cafe","Travel","Guest","Home","Office","Phone"});
    assert(online_profile_save(&lab,false));
    check(6,(const char *[]) {"Lab","Cafe","Travel","Guest","Home","Office"});
    assert(!online_profile_select("Phone") && !online_profile_select(NULL));
    assert(online_profile_select("Office"));
    check(6,(const char *[]) {"Office","Lab","Cafe","Travel","Guest","Home"});
    assert(online_profile_current(&active) && !memcmp(&active,&office,sizeof(active)));

    online_config_t updated=configuration("Guest",80);
    assert(online_profile_save(&updated,false));
    check(6,(const char *[]) {"Guest","Office","Lab","Cafe","Travel","Home"});
    assert(online_profile_current(&active) && !memcmp(&active,&updated,sizeof(active)));
    unsigned before=writes;
    assert(online_profile_select("Guest"));assert(writes==before);
    online_config_t invalid=office;invalid.version=99;assert(!online_profile_save(&invalid,false));
    profile_blob_t original=stored;
    fail_open=true;assert(!online_profile_save(&phone,false) && !online_profile_select("Office"));fail_open=false;
    fail_read=true;assert(!online_profile_save(&phone,false) && !online_profile_select("Office"));
    char names[ONLINE_PROFILE_LIMIT][33];unsigned count=88;
    assert(!online_profiles_summary(names,&count) && !count && !names[0][0]);fail_read=false;
    fail_set=true;
    assert(!online_profile_save(&phone,false) && !online_profile_select("Office"));
    assert(!online_profile_delete("Office",&was_current));
    fail_set=false;
    fail_commit=true;
    assert(!online_profile_save(&phone,false) && !online_profile_select("Office"));
    assert(!online_profile_delete("Guest",&was_current) && !was_current);
    fail_commit=false;
    assert(!memcmp(&original,&stored,sizeof(stored)) && !handles && writes==before);
    stored.count=ONLINE_PROFILE_LIMIT+1;assert(!online_profile_save(&office,false));stored=original;
    stored.entries[1]=stored.entries[0];assert(!online_profile_select("Lab"));stored=original;
    stored.entries[1].password[0]=0;memset(stored.entries[1].ssid,'x',sizeof(stored.entries[1].ssid));
    assert(!online_profile_current(&active));stored=original;
    stored_size--;assert(!online_profile_save(&office,false));stored_size=sizeof(stored);

    reenter=true;assert(online_profile_delete("Cafe",&was_current) && !was_current);reenter=false;
    check(5,(const char *[]) {"Guest","Office","Lab","Travel","Home"});
    assert(!online_profile_select("Cafe"));
    assert(online_profile_delete("Guest",&was_current) && was_current);
    assert(online_profile_current(&active) && !memcmp(&active,&office,sizeof(active)));
    assert(!online_profile_save(&updated,true)); /* A stale setup form cannot restore a deleted SSID. */
    assert(online_profile_save(&office,true));
    check(4,(const char *[]) {"Office","Lab","Travel","Home"});

    has_profiles=false;legacy.version=0;
    check(0,NULL);assert(!online_profile_current(&active));
    assert(online_profile_save(&office,false));check(1,(const char *[]) {"Office"});
    assert(legacy.version==0);
    assert(!handles);puts("Saved profiles: PASS");return 0;
}
