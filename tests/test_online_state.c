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
static void test_screen_idle(void) {
    online_screen_t pending={0};
    const unsigned modes[]={PTT_ARMED,PTT_ACTIVE,PTT_DRAINING};
    for(unsigned i=0;i<3;i++) {
        bool busy=online_standby_busy(ONLINE_READY,modes[i],false,false,false,false);
        assert(busy);
        assert(!online_screen_step(&pending,1000000+i*1000000,false,busy,60000));
    }
    assert(!online_standby_busy(ONLINE_LISTENING,PTT_IDLE,false,false,false,false));
    assert(!online_standby_busy(ONLINE_ERROR,PTT_FAILED,false,false,false,false));
    assert(online_standby_busy(ONLINE_READY,PTT_IDLE,true,false,false,false));
    assert(online_standby_busy(ONLINE_READY,PTT_IDLE,false,true,false,false));
    assert(online_standby_busy(ONLINE_LISTENING,PTT_IDLE,false,false,true,false));
    assert(online_standby_busy(ONLINE_READY,PTT_IDLE,false,false,false,true));
    assert(online_standby_busy(ONLINE_THINKING,PTT_IDLE,false,false,false,false));
    assert(online_standby_busy(ONLINE_SPEAKING,PTT_IDLE,false,false,false,false));
    assert(online_standby_busy(ONLINE_SETUP,PTT_IDLE,false,false,false,false));
    // Once an upload/reply/capture finishes, restart the complete idle interval.
    assert(!online_screen_step(&pending,3000001,false,true,60000));
    assert(!online_screen_step(&pending,3060000,false,false,60000));
    assert(online_screen_step(&pending,3060001,false,false,60000));
    const uint64_t timeouts[]={60000,180000,600000};
    for(unsigned i=0;i<3;i++) {
        uint64_t timeout=online_idle_timeout_ms((online_idle_t)i);
        assert(timeout==timeouts[i]);
        online_screen_t screen={0};
        assert(!online_screen_step(&screen,5000,false,false,timeout));
        assert(!online_screen_step(&screen,5000+timeout-1,false,false,timeout));
        assert(online_screen_step(&screen,5000+timeout,false,false,timeout));
        uint64_t wake=5000+timeout+1;
        assert(!online_screen_step(&screen,wake,true,false,timeout));
        assert(!online_screen_step(&screen,wake+timeout-1,false,false,timeout));
        assert(online_screen_step(&screen,wake+timeout,false,false,timeout));
        assert(!online_screen_step(&screen,wake+timeout+1,false,true,timeout));
        assert(!online_screen_step(&screen,wake+timeout*3,false,true,timeout));
        assert(online_screen_step(&screen,wake+timeout*4,false,false,timeout));
        /* Uptime over 32 bits must not shorten the selected duration. */
        uint64_t late=UINT64_C(5000000000);
        assert(!online_screen_step(&screen,late,true,false,timeout));
        assert(online_screen_step(&screen,late+timeout,false,false,timeout));
        assert(!online_screen_step(&screen,late+timeout+1,false,false,0));
        assert(!online_screen_step(&screen,late+timeout*100,false,false,0));
    }
    const online_idle_t choices[]={ONLINE_IDLE_1_MIN,ONLINE_IDLE_3_MIN,ONLINE_IDLE_10_MIN,ONLINE_IDLE_NEVER,ONLINE_IDLE_1_MIN};
    for(unsigned i=0;i<4;i++)assert(online_idle_next(choices[i])==choices[i+1]);
    assert(online_idle_timeout_ms(ONLINE_IDLE_NEVER)==0);
    assert(online_idle_normalize(255)==ONLINE_IDLE_1_MIN);
}
static void test_volume_steps(void) {
    const unsigned values[]={0,25,50,75,90,100,0};
    for(unsigned i=0;i+1<sizeof(values)/sizeof(values[0]);i++)assert(online_volume_next(values[i])==values[i+1]);
    assert(online_volume_next(55)==75);
    assert(online_volume_next(101)==0);
}
static void test_voice_feedback(void) {
    online_feedback_t f={0};char text[128];
    assert(online_feedback_progress(&f,"one","capturing",100));
    online_feedback_text(&f,10000,text,sizeof(text));assert(!text[0]);
    assert(online_feedback_progress(&f,"one","recognizing",200));
    assert(!online_feedback_progress(&f,"other","thinking",300));
    online_feedback_text(&f,8199,text,sizeof(text));assert(!text[0]);
    assert(online_feedback_progress(&f,"one","thinking",8000));
    online_feedback_text(&f,8200,text,sizeof(text));assert(strstr(text,"取消"));
    online_feedback_text(&f,11200,text,sizeof(text));assert(!text[0]);
    assert(online_feedback_result(&f,"one","failed","passport-no-speech"));
    assert(f.problem && f.settled);
    assert(!online_feedback_result(&f,"one","failed","passport-no-speech"));
    assert(!online_feedback_progress(&f,"one","thinking",9000));
    online_feedback_text(&f,20000,text,sizeof(text));assert(text[0]);
    assert(online_feedback_progress(&f,"two","capturing",21000) && !f.problem);
    assert(online_feedback_result(&f,"two","cancelled",""));assert(!f.problem);
    assert(online_error_recoverable("passport-asr-auth-failed"));
    assert(online_error_recoverable("passport-recognition-failed"));
    assert(!online_error_recoverable("passport-protocol-mismatch"));
    assert(!online_error_recoverable("passport-asr-unavailable"));
}
int main(void) {
    test_voice_feedback();
    assert(online_session_compatible("7.0.0",true,true));
    assert(!online_session_compatible("6.0.0",true,true));
    assert(!online_session_compatible(NULL,true,true));
    assert(!online_session_compatible("7.0.0",false,true));
    assert(!online_session_compatible("7.0.0",true,false));
    assert(!strstr(online_error_text("private-provider-key"),"private-provider-key"));
    test_time_wait();
    test_screen_idle();test_volume_steps();
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
    online_ptt_control_t talk={0};
    online_ptt_start_state(&talk,true);assert(atomic_load(&talk.problem)==1);
    atomic_store(&talk.supported,true);atomic_store(&talk.problem,0);
    online_ptt_arm_state(&talk);unsigned first=atomic_load(&talk.sequence);
    assert(online_ptt_frame_valid(&talk,first,false));
    assert(!online_ptt_frame_valid(&talk,first,true));
    online_ptt_release_state(&talk);
    assert(atomic_load(&talk.mode)==PTT_IDLE && !online_ptt_frame_valid(&talk,first,true));
    online_ptt_arm_state(&talk);unsigned second=atomic_load(&talk.sequence);
    assert(second!=first && !online_ptt_frame_valid(&talk,first,false));
    online_ptt_start_state(&talk,true);
    assert(online_ptt_frame_valid(&talk,second,true));
    online_ptt_release_state(&talk);
    assert(online_ptt_frame_valid(&talk,second,false) && online_ptt_frame_valid(&talk,second,true));
    atomic_store(&talk.ack,first);atomic_store(&talk.capture_stopped,true);
    assert(!online_ptt_commit_ready(&talk,second,true));
    atomic_store(&talk.ack,second);
    assert(!online_ptt_commit_ready(&talk,second,false) && online_ptt_commit_ready(&talk,second,true));
    online_ptt_fail_state(&talk,3);assert(atomic_load(&talk.problem)==3 && !online_ptt_commit_ready(&talk,second,true));
    online_ptt_release_state(&talk);assert(atomic_load(&talk.mode)==PTT_IDLE);
    online_ptt_arm_state(&talk);online_ptt_start_state(&talk,true);
    assert(online_ptt_release_waiting(&talk,true) && atomic_load(&talk.mode)==PTT_IDLE);
    online_ptt_arm_state(&talk);online_ptt_start_state(&talk,true);atomic_store(&talk.audio_started,true);
    assert(!online_ptt_release_waiting(&talk,true));online_ptt_release_state(&talk);
    assert(atomic_load(&talk.mode)==PTT_DRAINING);
    online_ptt_arm_state(&talk);online_ptt_start_state(&talk,true);
    assert(!online_ptt_release_waiting(&talk,false));online_ptt_release_state(&talk);
    assert(atomic_load(&talk.mode)==PTT_DRAINING);
    online_menu_t menu={0};
    assert(online_menu_key(&menu,ONLINE_KEY_LONG)==ONLINE_ACTION_NONE);
    assert(menu.page==ONLINE_SETTINGS && menu.selected==0);
    assert(online_menu_key(&menu,ONLINE_KEY_UP_LONG)==ONLINE_ACTION_NONE);
    assert(menu.selected==ONLINE_MENU_HOME);
    assert(online_menu_key(&menu,ONLINE_KEY_DOWN_LONG)==ONLINE_ACTION_NONE);
    assert(menu.selected==ONLINE_MENU_DISPLAY);
    online_menu_key(&menu,ONLINE_KEY_DOWN);
    assert(menu.selected==ONLINE_MENU_STATUS);
    online_menu_key(&menu,ONLINE_KEY_DOWN_LONG);
    assert(menu.selected==ONLINE_MENU_UPLOAD);
    online_menu_key(&menu,ONLINE_KEY_UP_LONG);
    assert(menu.selected==ONLINE_MENU_STATUS);
    online_menu_key(&menu,ONLINE_KEY_UP);
    assert(online_menu_key(&menu,ONLINE_KEY_OK)==ONLINE_ACTION_NONE);
    assert(menu.page==ONLINE_DISPLAY);
    assert(online_menu_key(&menu,ONLINE_KEY_OK)==ONLINE_ACTION_THEME);
    online_menu_key(&menu,ONLINE_KEY_DOWN);
    assert(online_menu_key(&menu,ONLINE_KEY_OK)==ONLINE_ACTION_BACKGROUND);
    online_menu_key(&menu,ONLINE_KEY_DOWN);
    assert(online_menu_key(&menu,ONLINE_KEY_OK)==ONLINE_ACTION_BRIGHTNESS);
    online_menu_key(&menu,ONLINE_KEY_DOWN);
    assert(online_menu_key(&menu,ONLINE_KEY_OK)==ONLINE_ACTION_IDLE);
    assert(menu.page==ONLINE_DISPLAY && menu.selected==ONLINE_DISPLAY_IDLE);
    online_menu_key(&menu,ONLINE_KEY_DOWN);
    assert(online_menu_key(&menu,ONLINE_KEY_OK)==ONLINE_ACTION_OVERLAY);
    online_menu_key(&menu,ONLINE_KEY_DOWN);
    online_menu_key(&menu,ONLINE_KEY_OK);
    assert(menu.page==ONLINE_SETTINGS);
    online_menu_key(&menu,ONLINE_KEY_DOWN);
    assert(online_menu_key(&menu,ONLINE_KEY_OK)==ONLINE_ACTION_NONE && menu.page==ONLINE_STATUS);
    online_menu_key(&menu,ONLINE_KEY_DOWN);assert(menu.page==ONLINE_STATUS && menu.selected==0);
    online_menu_key(&menu,ONLINE_KEY_OK);assert(menu.page==ONLINE_SETTINGS && menu.selected==ONLINE_MENU_STATUS);
    online_menu_key(&menu,ONLINE_KEY_DOWN);
    assert(menu.selected==2 && online_menu_key(&menu,ONLINE_KEY_OK)==ONLINE_ACTION_NONE && menu.page==ONLINE_SOUND);
    assert(online_menu_key(&menu,ONLINE_KEY_OK)==ONLINE_ACTION_VOLUME);
    online_menu_key(&menu,ONLINE_KEY_DOWN);
    assert(online_menu_key(&menu,ONLINE_KEY_OK)==ONLINE_ACTION_CUE);
    online_menu_key(&menu,ONLINE_KEY_DOWN);
    online_menu_key(&menu,ONLINE_KEY_OK);
    assert(menu.page==ONLINE_SETTINGS && menu.selected==ONLINE_MENU_SOUND);
    online_menu_key(&menu,ONLINE_KEY_DOWN);
    assert(online_menu_key(&menu,ONLINE_KEY_OK)==ONLINE_ACTION_SETUP);
    assert(menu.page==ONLINE_HOME);
    online_menu_key(&menu,ONLINE_KEY_LONG);
    for(unsigned i=0;i<ONLINE_MENU_HOME;i++)online_menu_key(&menu,ONLINE_KEY_DOWN);
    assert(online_menu_key(&menu,ONLINE_KEY_OK)==ONLINE_ACTION_NONE);
    assert(menu.page==ONLINE_HOME);
    online_menu_key(&menu,ONLINE_KEY_LONG);
    online_menu_key(&menu,ONLINE_KEY_UP);
    assert(menu.selected==5);
    online_menu_key(&menu,ONLINE_KEY_OK);
    assert(menu.page==ONLINE_CONNECTION && menu.selected==0);
    assert(online_menu_key(&menu,ONLINE_KEY_OK)==ONLINE_ACTION_SETUP && menu.page==ONLINE_HOME);
    online_menu_key(&menu,ONLINE_KEY_LONG);
    online_menu_key(&menu,ONLINE_KEY_UP);
    menu.network_count=3;
    online_menu_key(&menu,ONLINE_KEY_OK);
    assert(menu.page==ONLINE_CONNECTION);
    assert(online_menu_key(&menu,ONLINE_KEY_OK)==ONLINE_ACTION_NONE); /* Already selected. */
    online_menu_key(&menu,ONLINE_KEY_UP);assert(menu.selected==4);
    online_menu_key(&menu,ONLINE_KEY_OK);
    assert(menu.page==ONLINE_SETTINGS && menu.selected==ONLINE_MENU_CONNECTION);
    online_menu_key(&menu,ONLINE_KEY_OK);
    online_menu_key(&menu,ONLINE_KEY_DOWN);
    assert(online_menu_key(&menu,ONLINE_KEY_OK)==ONLINE_ACTION_NETWORK && menu.selected==1);
    online_menu_key(&menu,ONLINE_KEY_DOWN);
    online_menu_key(&menu,ONLINE_KEY_DOWN);
    assert(menu.selected==3);
    assert(online_menu_key(&menu,ONLINE_KEY_OK)==ONLINE_ACTION_SETUP);
    assert(menu.page==ONLINE_HOME);
    menu.provisioning=true;
    assert(online_menu_key(&menu,ONLINE_KEY_OK)==ONLINE_ACTION_RESUME);
    online_menu_key(&menu,ONLINE_KEY_LONG);
    for(unsigned i=0;i<ONLINE_MENU_HOME;i++)online_menu_key(&menu,ONLINE_KEY_DOWN);
    assert(online_menu_key(&menu,ONLINE_KEY_OK)==ONLINE_ACTION_RESUME && menu.page==ONLINE_HOME);
    online_menu_key(&menu,ONLINE_KEY_LONG);
    online_menu_key(&menu,ONLINE_KEY_UP);
    online_menu_key(&menu,ONLINE_KEY_OK);
    assert(menu.page==ONLINE_CONNECTION && menu.selected==0);
    assert(online_menu_key(&menu,ONLINE_KEY_OK)==ONLINE_ACTION_RESUME);
    assert(online_menu_key(&menu,ONLINE_KEY_LONG)==ONLINE_ACTION_RESUME && menu.page==ONLINE_HOME);
    menu.network_count=0;
    assert(online_menu_key(&menu,ONLINE_KEY_OK)==ONLINE_ACTION_NONE);
    online_menu_key(&menu,ONLINE_KEY_LONG);
    assert(online_menu_key(&menu,ONLINE_KEY_LONG)==ONLINE_ACTION_NONE && menu.page==ONLINE_HOME);
    menu.provisioning=false;
    online_menu_key(&menu,ONLINE_KEY_LONG);online_menu_key(&menu,ONLINE_KEY_UP);
    online_menu_key(&menu,ONLINE_KEY_OK);online_menu_key(&menu,ONLINE_KEY_LONG);
    assert(menu.page==ONLINE_HOME);
    assert(online_theme_normalize(255)==ONLINE_THEME_WHALE);
    assert(online_theme_normalize(ONLINE_THEME_MINT)==ONLINE_THEME_MINT);
    assert(online_theme_next(ONLINE_THEME_WHALE)==ONLINE_THEME_MINT);
    assert(online_theme_next(ONLINE_THEME_MINT)==ONLINE_THEME_WHALE_MAID);
    assert(online_theme_next(ONLINE_THEME_WHALE_MAID)==ONLINE_THEME_CLAUDE);
    assert(online_theme_next(ONLINE_THEME_CLAUDE)==ONLINE_THEME_WHALE);
    assert(online_background_normalize(255)==ONLINE_BACKGROUND_DAY);
    assert(online_background_next(ONLINE_BACKGROUND_DAY,false)==ONLINE_BACKGROUND_NIGHT);
    assert(online_background_next(ONLINE_BACKGROUND_NIGHT,false)==ONLINE_BACKGROUND_SPRING);
    assert(online_background_next(ONLINE_BACKGROUND_SPRING,false)==ONLINE_BACKGROUND_SUMMER);
    assert(online_background_next(ONLINE_BACKGROUND_SUMMER,false)==ONLINE_BACKGROUND_AUTUMN);
    assert(online_background_next(ONLINE_BACKGROUND_AUTUMN,false)==ONLINE_BACKGROUND_WINTER);
    assert(online_background_next(ONLINE_BACKGROUND_WINTER,false)==ONLINE_BACKGROUND_OFF);
    assert(online_background_next(ONLINE_BACKGROUND_OFF,false)==ONLINE_BACKGROUND_DAY);
    assert(online_background_next(ONLINE_BACKGROUND_WINTER,true)==ONLINE_BACKGROUND_CUSTOM);
    assert(online_background_next(ONLINE_BACKGROUND_CUSTOM,true)==ONLINE_BACKGROUND_OFF);
    assert(online_background_next(ONLINE_BACKGROUND_OFF,true)==ONLINE_BACKGROUND_DAY);
    assert(online_background_next(ONLINE_BACKGROUND_CUSTOM,false)==ONLINE_BACKGROUND_OFF);
    assert(online_background_normalize(ONLINE_BACKGROUND_OFF)==ONLINE_BACKGROUND_OFF);
    const unsigned brightness_levels[]={25,50,75,100};
    for(unsigned i=0;i<4;i++) {
        assert(online_brightness_normalize(brightness_levels[i])==brightness_levels[i]);
        assert(online_brightness_next(brightness_levels[i])==brightness_levels[(i+1)%4]);
    }
    assert(online_brightness_normalize(0)==100 && online_brightness_normalize(255)==100);
    bool expressions[ONLINE_AVATAR_COUNT]={0};
    unsigned changes=0;online_avatar_t previous=ONLINE_AVATAR_THINKING;
    for(uint64_t now=0;now<60000;now+=50) {
        online_avatar_t frame=online_avatar_frame(ONLINE_THINKING,true,0,now);
        expressions[frame]=true;if(frame!=previous)changes++;previous=frame;
        assert(frame!=ONLINE_AVATAR_SPEAKING && frame!=ONLINE_AVATAR_ERROR);
        assert(frame==online_avatar_frame(ONLINE_THINKING,true,0,now+3200));
    }
    assert(expressions[ONLINE_AVATAR_THINKING] && expressions[ONLINE_AVATAR_BLINK] && expressions[ONLINE_AVATAR_LISTENING]);
    assert(changes>20 && changes<100);
    for(unsigned phase=0;phase<=ONLINE_ERROR;phase++)for(unsigned mic=0;mic<2;mic++) {
        for(uint64_t now=0;now<10000;now+=137) {
            online_avatar_t frame=online_avatar_frame((online_phase_t)phase,mic!=0,65535,now);
            assert(frame>=ONLINE_AVATAR_IDLE && frame<ONLINE_AVATAR_COUNT);
        }
    }
    puts("Online configuration, theme navigation and frame bounds: PASS");
}
