#include "online_state.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
static void test_time_wait(void) {
    online_time_wait_t w={0};
    assert(online_time_step(&w,false,true,0)==ONLINE_TIME_IDLE);
    assert(online_time_step(&w,true,false,0)==ONLINE_TIME_IDLE);
    assert(online_time_step(&w,true,true,0)==ONLINE_TIME_START && w.source_index==0);
    assert(online_time_step(&w,true,true,9999)==ONLINE_TIME_WAIT && !w.timed_out);
    assert(online_time_step(&w,true,true,10000)==ONLINE_TIME_RETRY && w.timed_out && w.source_index==1);
    assert(online_time_step(&w,true,true,10001)==ONLINE_TIME_WAIT && w.timed_out);
    assert(online_time_step(&w,true,true,19999)==ONLINE_TIME_WAIT);
    assert(online_time_step(&w,true,true,20000)==ONLINE_TIME_RETRY && w.source_index==2);
    assert(online_time_step(&w,true,true,30000)==ONLINE_TIME_RETRY && w.source_index==0);
    assert(online_time_step(&w,false,true,30001)==ONLINE_TIME_IDLE && !w.active && !w.timed_out);
    assert(online_time_step(&w,true,true,400000)==ONLINE_TIME_START && !w.timed_out && w.source_index==0);
    assert(online_time_step(&w,true,false,400001)==ONLINE_TIME_IDLE && !w.active);
    assert(online_time_step(&w,true,true,UINT64_C(5000000000))==ONLINE_TIME_START);
    assert(online_time_step(&w,true,true,UINT64_C(5000010000))==ONLINE_TIME_RETRY);
}
int main(void) {
    assert(online_session_compatible("7.0.0",true,true));
    assert(!online_session_compatible("6.0.0",true,true));
    assert(!online_session_compatible(NULL,true,true));
    assert(!online_session_compatible("7.0.0",false,true));
    assert(!online_session_compatible("7.0.0",true,false));
    assert(!strstr(online_error_text("private-provider-key"),"private-provider-key"));
    test_time_wait();
    assert(!online_playback_prefill_wait(0,0));
    assert(online_playback_prefill_wait(1,0));
    assert(online_playback_prefill_wait(23,349));
    assert(!online_playback_prefill_wait(24,0));
    assert(!online_playback_prefill_wait(1,350));
    assert(!online_playback_prefill_wait(32,1000));
    online_config_t c={.version=1,.ssid="network",.password="example-password",.url="wss://voice.example.com/api/realtime",.token="example-access-token-change-me"};
    assert(online_config_valid(&c));
    strcpy(c.url,"ws://192.168.1.2:3101/api/realtime");assert(online_config_valid(&c));
    strcpy(c.url,"wss://user:secret@voice.example.com/api/realtime");assert(!online_config_valid(&c));
    strcpy(c.url,"wss:///api/realtime");assert(!online_config_valid(&c));
    strcpy(c.url,"wss://voice.example.com/api/realtime?token=secret");assert(!online_config_valid(&c));
    strcpy(c.url,"wss://voice.example.com/api/realtime");strcpy(c.token,"bad\r\nInjected: header-value");assert(!online_config_valid(&c));
    memset(c.token,'x',sizeof(c.token));assert(!online_config_valid(&c));
    c.token[0]=0;assert(!online_config_valid(&c));
    strcpy(c.url,"ws://192.168.1.2:3101/api/realtime");strcpy(c.token,"0042");assert(online_config_valid(&c));
    online_config_t next={.version=1,.url="ws://192.168.1.3:3101/api/realtime",.token="1234"};
    assert(online_reuse_wifi(&next,&c));
    assert(!strcmp(next.ssid,c.ssid) && !strcmp(next.password,c.password));
    assert(!strcmp(next.token,"1234") && !strcmp(next.url,"ws://192.168.1.3:3101/api/realtime"));
    assert(online_config_valid(&next));
    online_config_t broken={0};assert(!online_reuse_wifi(&next,&broken));
    strcpy(c.token,"123");assert(!online_config_valid(&c));
    strcpy(c.token,"12a4");assert(!online_config_valid(&c));
    strcpy(c.token,"12345");assert(!online_config_valid(&c));
    strcpy(c.token,"0042");strcpy(c.url,"wss://voice.example.com/api/realtime");assert(!online_config_valid(&c));
    strcpy(c.url,"ws://198.51.100.2:3101/api/realtime");assert(!online_config_valid(&c));
    assert(strcmp(online_connection_error(401,0,true),online_connection_error(0,ECONNREFUSED,true)));
    assert(strcmp(online_connection_error(0,ETIMEDOUT,true),online_connection_error(0,0,false)));
    char endpoint[80];
    assert(online_endpoint_from_ip("192.0.2.10",endpoint,sizeof(endpoint)));
    assert(!strcmp(endpoint,"wss://192.0.2.10:3101/api/realtime"));
    const char *lan[]={"10.0.0.1","172.16.0.1","172.31.255.254","192.168.1.2"};
    for(unsigned i=0;i<sizeof(lan)/sizeof(lan[0]);i++) {
        assert(online_endpoint_from_ip(lan[i],endpoint,sizeof(endpoint)));
        assert(!strncmp(endpoint,"ws://",5));
    }
    const char *public_ips[]={"192.0.2.10","172.15.1.1","172.32.1.1","198.51.100.1"};
    for(unsigned i=0;i<sizeof(public_ips)/sizeof(public_ips[0]);i++) {
        assert(online_endpoint_from_ip(public_ips[i],endpoint,sizeof(endpoint)));
        assert(!strncmp(endpoint,"wss://",6));
    }
    const char *bad[]={"0.0.0.0","127.0.0.1","255.255.255.255","224.0.0.1","","192.168.1.256","192.168.1","1.2.3.4.5","1..2.3","1.2.3.4:80","ws://1.2.3.4","1.2.3.4/path","1.2.3.-1","192.168.001.1"};
    for(unsigned i=0;i<sizeof(bad)/sizeof(bad[0]);i++)assert(!online_endpoint_from_ip(bad[i],endpoint,sizeof(endpoint)));
    assert(!online_endpoint_from_ip("1.2.3.4",endpoint,8));
    assert(online_endpoint_from_ip_port("192.168.1.2",4100,endpoint,sizeof(endpoint)));
    assert(!strcmp(endpoint,"ws://192.168.1.2:4100/api/realtime"));
    assert(!online_endpoint_from_ip_port("192.168.1.2",0,endpoint,sizeof(endpoint)));
    assert(!online_endpoint_from_ip_port("192.168.1.2",65536,endpoint,sizeof(endpoint)));
    size_t n=0;assert(online_fragment(&n,20,0,5,12));assert(n==5);
    assert(online_fragment(&n,20,5,7,12));assert(n==12);
    assert(!online_fragment(&n,20,0,20,20));assert(n==0);
    assert(!online_fragment(&n,20,1,3,12));
    assert(online_fragment(&n,20,0,4,12));assert(!online_fragment(&n,20,4,9,12));
    assert(!online_fragment(&n,20,0,(size_t)-1,12));
    online_menu_t menu={0};
    assert(online_menu_key(&menu,ONLINE_KEY_LONG)==ONLINE_ACTION_NONE);
    assert(menu.page==ONLINE_SETTINGS && menu.selected==0);
    online_menu_key(&menu,ONLINE_KEY_DOWN);
    assert(online_menu_key(&menu,ONLINE_KEY_OK)==ONLINE_ACTION_NONE);
    assert(menu.page==ONLINE_CONFIRM && menu.selected==0);
    assert(online_menu_key(&menu,ONLINE_KEY_OK)==ONLINE_ACTION_NONE);
    assert(menu.page==ONLINE_SETTINGS);
    online_menu_key(&menu,ONLINE_KEY_OK);
    online_menu_key(&menu,ONLINE_KEY_DOWN);
    assert(online_menu_key(&menu,ONLINE_KEY_OK)==ONLINE_ACTION_SETUP);
    assert(menu.page==ONLINE_HOME);
    online_menu_key(&menu,ONLINE_KEY_LONG);online_menu_key(&menu,ONLINE_KEY_DOWN);
    online_menu_key(&menu,ONLINE_KEY_OK);online_menu_key(&menu,ONLINE_KEY_LONG);
    assert(menu.page==ONLINE_HOME);
    puts("Online configuration and frame bounds: PASS");
}
