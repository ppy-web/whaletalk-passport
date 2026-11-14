#include "online_profiles.h"
#include "nvs.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static online_profiles_t stored,pending;
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
    assert(*size>=len);memcpy(out,data,len);*size=len;return ESP_OK;
}
esp_err_t nvs_set_blob(nvs_handle_t handle,const char *key,const void *data,size_t size) {
    assert(handle && handles && !strcmp(key,"profiles") && size==sizeof(pending));
    if(reenter)assert(!online_profile_select("Office")); /* HTTP and settings cannot interleave writes. */
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
static void check(const char *a,const char *b,const char *c) {
    char names[ONLINE_PROFILE_LIMIT][33];unsigned count=99;
    assert(online_profiles_summary(names,&count));
    assert(count==(unsigned)(!!a+!!b+!!c));
    const char *expected[]={a,b,c};
    for(unsigned i=0;i<count;i++)assert(!strcmp(names[i],expected[i]));
    assert(!handles);
}
int main(void) {
    check(NULL,NULL,NULL);online_config_t active;
    assert(!online_profile_current(&active) && !active.ssid[0]);assert(!online_profile_select("Home"));
    legacy=configuration("Home",10);has_legacy=true;
    check("Home",NULL,NULL);assert(!has_profiles && !writes);
    online_config_t b=configuration("Office",20),c=configuration("Phone",30),d=configuration("Guest",40);
    assert(online_profile_save(&b));check("Office","Home",NULL);
    assert(!memcmp(&legacy.ssid,"Home",5)); /* Existing version-one blob is preserved. */
    assert(online_profile_save(&c));assert(online_profile_save(&d));check("Guest","Phone","Office");
    assert(online_profile_select("Office"));check("Office","Guest","Phone");
    assert(online_profile_current(&active) && !memcmp(&active,&b,sizeof(active)));
    d=configuration("Guest",50);assert(online_profile_save(&d));check("Guest","Office","Phone");
    assert(online_profile_current(&active) && !memcmp(&active,&d,sizeof(active)));
    unsigned before=writes;
    assert(online_profile_select("Guest"));assert(writes==before);
    assert(!online_profile_select("Home") && !online_profile_select(NULL)); /* Evicted entry. */
    online_config_t invalid=b;invalid.version=99;assert(!online_profile_save(&invalid));assert(writes==before);
    online_profiles_t original=stored;
    fail_open=true;assert(!online_profile_save(&c) && !online_profile_select("Office"));fail_open=false;
    fail_read=true;assert(!online_profile_save(&c) && !online_profile_select("Office"));
    char names[ONLINE_PROFILE_LIMIT][33];unsigned count=88;
    assert(!online_profiles_summary(names,&count) && !count && !names[0][0]);fail_read=false;
    fail_set=true;assert(!online_profile_save(&c) && !online_profile_select("Office"));fail_set=false;
    fail_commit=true;assert(!online_profile_save(&c) && !online_profile_select("Office"));fail_commit=false;
    assert(!memcmp(&original,&stored,sizeof(stored)) && !handles && writes==before);
    stored.count=4;assert(!online_profile_save(&b));stored=original;
    stored.entries[1]=stored.entries[0];assert(!online_profile_select("Phone"));stored=original;
    stored.entries[1].password[0]=0;memset(stored.entries[1].ssid,'x',sizeof(stored.entries[1].ssid));
    assert(!online_profile_current(&active));stored=original;
    stored_size--;assert(!online_profile_save(&b));stored_size=sizeof(stored);
    reenter=true;assert(online_profile_select("Phone"));reenter=false;check("Phone","Guest","Office");
    assert(online_profile_save(&legacy));check("Home","Phone","Guest");
    has_profiles=false;legacy.version=0;
    check(NULL,NULL,NULL);assert(!online_profile_current(&active));
    assert(online_profile_save(&b));check("Office",NULL,NULL);
    assert(legacy.version==0);
    assert(!handles);puts("Saved profiles: PASS");return 0;
}
