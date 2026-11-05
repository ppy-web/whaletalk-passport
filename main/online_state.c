#include "online_state.h"
#include <string.h>
#include <stdio.h>
#include <errno.h>
static bool bounded(const char *s,size_t n) { return memchr(s,0,n)!=NULL; }
bool online_config_valid(const online_config_t *c) {
    if(!c || c->version!=1 || !bounded(c->ssid,sizeof(c->ssid)) ||
       !bounded(c->password,sizeof(c->password)) || !bounded(c->url,sizeof(c->url)) ||
       !bounded(c->token,sizeof(c->token)) || !c->ssid[0]) return false;
    size_t pass=strlen(c->password);
    if(pass && (pass<8 || pass>63)) return false;
    const char *host=NULL;
    if(!strncmp(c->url,"ws://",5)) host=c->url+5;
    if(!strncmp(c->url,"wss://",6)) host=c->url+6;
    size_t token_length=strlen(c->token);
    bool pairing=token_length==4 && strspn(c->token,"0123456789")==4;
    if(!host || !*host || *host=='/' || (!pairing && token_length<24)) return false;
    if(strpbrk(c->url,"@?#\r\n\t ") || strpbrk(c->token,"\r\n\t ")) return false;
    const char *path=strchr(host,'/');
    if(!path || strcmp(path,"/api/realtime")) return false;
    if(pairing) {
        unsigned a,b,d,e,port;int used=0;
        if(sscanf(c->url,"ws://%u.%u.%u.%u:%u/api/realtime%n",&a,&b,&d,&e,&port,&used)!=5 ||
           (size_t)used!=strlen(c->url) || a>255 || b>255 || d>255 || e>255 || !port || port>65535 ||
           !(a==10 || (a==172 && b>=16 && b<=31) || (a==192 && b==168)))return false;
    }
    return true;
}
bool online_reuse_wifi(online_config_t *next,const online_config_t *previous) {
    if(!next || !online_config_valid(previous))return false;
    memcpy(next->ssid,previous->ssid,sizeof(next->ssid));
    memcpy(next->password,previous->password,sizeof(next->password));
    return true;
}
bool online_fragment(size_t *used,size_t cap,size_t off,size_t len,size_t total) {
    if(!off) *used=0;
    if(total>=cap || off!=*used || off>total || len>total-off) { *used=0;return false; }
    *used+=len; return true;
}
const char *online_phase_text(online_phase_t p) {
    static const char *const names[]={"配置后端","正在连接","准备好了","你说，我在听","思考一下","正在回答","连接失败"};
    return p<=ONLINE_ERROR?names[p]:names[ONLINE_ERROR];
}

bool online_session_compatible(const char *version, bool input_audio, bool receipts) {
    return version && !strcmp(version,"7.0.0") && input_audio && receipts;
}

const char *online_error_text(const char *code) {
    if(!code)return "后端错误，请检查服务配置";
    if(!strcmp(code,"passport-asr-unavailable"))return "请先准备 DSH 语音识别";
    if(!strcmp(code,"passport-recognition-failed"))return "语音识别失败，请重新说话";
    if(!strcmp(code,"passport-speech-failed"))return "请检查 DSH 语音合成";
    if(!strcmp(code,"passport-request-timeout"))return "等待超时，请重新说话";
    if(!strcmp(code,"passport-session-unavailable") || !strcmp(code,"passport-preset-unavailable"))return "请检查 DSH 对话配置";
    if(!strcmp(code,"passport-preset-changed"))return "请在 DSH 刷新预设";
    return "后端错误，请检查服务配置";
}

const char *online_connection_error(int http_status,int socket_error,bool has_ip) {
    if(!has_ip)return "Wi-Fi 断开，正在重连";
    if(http_status==401 || http_status==403)return "配对码不一致\n请核对电脑与设备";
    if(http_status==404)return "连接端口错误\n请核对电脑端口";
    if(http_status==503)return "连接暂不可用\n请稍后重试";
    if(socket_error==ECONNREFUSED)return "电脑未开启连接\n请在 DSH 开启连接器";
    if(socket_error==ETIMEDOUT)return "连接电脑超时\n请检查 IP 与防火墙";
    return "无法连接电脑\n请核对 IP 与连接器";
}

bool online_endpoint_from_ip(const char *ip,char *out,size_t size) {
    return online_endpoint_from_ip_port(ip,3101,out,size);
}
bool online_endpoint_from_ip_port(const char *ip,unsigned port,char *out,size_t size) {
    if(!ip || !*ip || strlen(ip)>15 || !out || !size || !port || port>65535)return false;
    unsigned parts=0,value=0,digits=0,octets[4]={0};
    for(const char *p=ip;;p++) {
        if(*p>='0' && *p<='9') {
            if(digits==1 && value==0)return false;
            if(++digits>3)return false;
            value=value*10+(unsigned)(*p-'0');if(value>255)return false;
        } else if(*p=='.' || !*p) {
            if(!digits || ++parts>4)return false;
            octets[parts-1]=value;
            if(!*p)break;
            value=digits=0;
        } else return false;
    }
    if(parts!=4)return false;
    if(octets[0]==0 || octets[0]==127 || octets[0]>=224)return false;
    bool lan=octets[0]==10 || (octets[0]==172 && octets[1]>=16 && octets[1]<=31) ||
        (octets[0]==192 && octets[1]==168);
    int n=snprintf(out,size,"%s://%s:%u/api/realtime",lan?"ws":"wss",ip,port);
    return n>0 && (size_t)n<size;
}

online_theme_t online_theme_normalize(unsigned stored) {
    return stored==ONLINE_THEME_MINT?ONLINE_THEME_MINT:ONLINE_THEME_WHALE;
}
online_background_t online_background_normalize(unsigned stored) {
    return stored<=ONLINE_BACKGROUND_CUSTOM?(online_background_t)stored:ONLINE_BACKGROUND_DAY;
}
online_background_t online_background_next(online_background_t current,bool has_custom) {
    unsigned count=has_custom?3:2;
    return (online_background_t)((online_background_normalize(current)+1)%count);
}
unsigned online_volume_next(unsigned current) {
    static const unsigned steps[]={0,25,50,75,90,100};
    for(unsigned i=0;i<sizeof(steps)/sizeof(steps[0]);i++)if(steps[i]>current)return steps[i];
    return 0;
}
unsigned online_brightness_normalize(unsigned stored) {
    return stored==25 || stored==50 || stored==75 || stored==100?stored:100;
}
unsigned online_brightness_next(unsigned current) {
    return online_brightness_normalize(current)%100+25;
}
online_idle_t online_idle_normalize(unsigned stored) {
    return stored<=ONLINE_IDLE_NEVER?(online_idle_t)stored:ONLINE_IDLE_1_MIN;
}
online_idle_t online_idle_next(online_idle_t current) {
    return (online_idle_t)((online_idle_normalize(current)+1)%4);
}
uint64_t online_idle_timeout_ms(online_idle_t setting) {
    static const uint64_t values[]={60000,180000,600000,0};
    return values[online_idle_normalize(setting)];
}
bool online_screen_step(online_screen_t *screen,uint64_t now_ms,bool activity,bool keep_awake,uint64_t timeout_ms) {
    if(!screen->initialized || activity || keep_awake || !timeout_ms) {
        screen->initialized=true;screen->activity_ms=now_ms;screen->asleep=false;
    } else if(now_ms-screen->activity_ms>=timeout_ms)screen->asleep=true;
    return screen->asleep;
}
online_theme_t online_theme_next(online_theme_t theme) {
    return theme==ONLINE_THEME_WHALE?ONLINE_THEME_MINT:ONLINE_THEME_WHALE;
}
online_avatar_t online_avatar_frame(online_phase_t phase,bool mic,unsigned level,uint64_t now_ms) {
    if(phase==ONLINE_ERROR)return ONLINE_AVATAR_ERROR;
    if(phase==ONLINE_THINKING) {
        /* Bounded 3.2 s loop, at most ~3 frame changes/s. No dependency on
         * server traffic, no return to listening or fabricated completion. */
        unsigned tick=now_ms%3200;
        if(tick>=2200 && tick<2360)return ONLINE_AVATAR_BLINK;
        return tick>=1400 && tick<1800?ONLINE_AVATAR_LISTENING:ONLINE_AVATAR_THINKING;
    }
    if(phase==ONLINE_SPEAKING)return level>1800 && (now_ms/160)%2?ONLINE_AVATAR_SPEAKING:ONLINE_AVATAR_IDLE;
    if(mic && (phase==ONLINE_LISTENING || phase==ONLINE_READY))return ONLINE_AVATAR_LISTENING;
    return now_ms%4500<120?ONLINE_AVATAR_BLINK:ONLINE_AVATAR_IDLE;
}

online_action_t online_menu_key(online_menu_t *m,online_key_t key) {
    if(key==ONLINE_KEY_LONG) {
        m->page=m->page==ONLINE_HOME?ONLINE_SETTINGS:ONLINE_HOME;m->selected=0;
        return ONLINE_ACTION_NONE;
    }
    if(m->page==ONLINE_HOME) {
        if(key==ONLINE_KEY_OK)return ONLINE_ACTION_MIC;
        if(key==ONLINE_KEY_UP)return ONLINE_ACTION_VOLUME;
        if(key==ONLINE_KEY_DOWN)return ONLINE_ACTION_CANCEL;
    } else if(key==ONLINE_KEY_UP || key==ONLINE_KEY_DOWN) {
        unsigned count=m->page==ONLINE_SETTINGS?ONLINE_MENU_COUNT:2;
        m->selected=(m->selected+(key==ONLINE_KEY_DOWN?1:count-1))%count;
    }
    else if(key==ONLINE_KEY_OK) {
        if(m->page==ONLINE_SETTINGS) {
            if(m->selected==ONLINE_MENU_THEME)return ONLINE_ACTION_THEME;
            if(m->selected==ONLINE_MENU_BACKGROUND)return ONLINE_ACTION_BACKGROUND;
            if(m->selected==ONLINE_MENU_BRIGHTNESS)return ONLINE_ACTION_BRIGHTNESS;
            if(m->selected==ONLINE_MENU_IDLE)return ONLINE_ACTION_IDLE;
            if(m->selected==ONLINE_MENU_UPLOAD) {
                m->page=ONLINE_HOME;m->selected=0;return ONLINE_ACTION_SETUP;
            }
            m->page=m->selected==ONLINE_MENU_SETUP?ONLINE_CONFIRM:ONLINE_HOME;m->selected=0;
        } else if(m->selected) {
            m->page=ONLINE_HOME;m->selected=0;return ONLINE_ACTION_SETUP;
        } else {m->page=ONLINE_SETTINGS;m->selected=ONLINE_MENU_SETUP;}
    }
    return ONLINE_ACTION_NONE;
}

bool online_playback_prefill_wait(unsigned queued,unsigned elapsed_ms) {
    return queued>0 && queued<24 && elapsed_ms<350;
}

online_time_action_t online_time_step(online_time_wait_t *w, bool has_ip,
                                     bool needs_time, uint64_t now_ms) {
    if (!has_ip || !needs_time) { memset(w,0,sizeof(*w)); return ONLINE_TIME_IDLE; }
    if (!w->active) {
        w->active=true; w->started_ms=w->retry_ms=now_ms;
        return ONLINE_TIME_START;
    }
    w->timed_out=now_ms-w->started_ms>=10000;
    if (now_ms-w->retry_ms>=10000) {
        w->retry_ms=now_ms;
        w->source_index=(w->source_index+1)%3;
        return ONLINE_TIME_RETRY;
    }
    return ONLINE_TIME_WAIT;
}
