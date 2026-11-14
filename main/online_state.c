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
void online_ptt_arm_state(online_ptt_control_t *p) {
    unsigned sequence=atomic_fetch_add(&p->sequence,1)+1;
    if(!sequence)atomic_fetch_add(&p->sequence,1);
    atomic_store(&p->ack,0);atomic_store(&p->audio_started,false);atomic_store(&p->capture_stopped,false);
    atomic_store(&p->mode,PTT_ARMED);
}
void online_ptt_start_state(online_ptt_control_t *p,bool ready) {
    unsigned expected=PTT_ARMED;
    if(!atomic_compare_exchange_strong(&p->mode,&expected,PTT_ACTIVE) && expected!=PTT_FAILED)
        atomic_store(&p->problem,ready && !atomic_load(&p->supported)?1:2);
}
void online_ptt_release_state(online_ptt_control_t *p) {
    unsigned expected=PTT_ARMED;
    if(atomic_compare_exchange_strong(&p->mode,&expected,PTT_IDLE))return;
    expected=PTT_ACTIVE;
    if(atomic_compare_exchange_strong(&p->mode,&expected,PTT_DRAINING))return;
    if(expected==PTT_FAILED)atomic_store(&p->mode,PTT_IDLE);
}
bool online_ptt_release_waiting(online_ptt_control_t *p,bool wait_cue) {
    if(!wait_cue || atomic_load(&p->audio_started))return false;
    unsigned expected=PTT_ACTIVE;
    return atomic_compare_exchange_strong(&p->mode,&expected,PTT_IDLE);
}
void online_ptt_fail_state(online_ptt_control_t *p,unsigned problem) {
    atomic_store(&p->problem,problem);atomic_store(&p->mode,PTT_FAILED);
}
bool online_ptt_frame_valid(const online_ptt_control_t *p,unsigned sequence,bool sending) {
    unsigned mode=atomic_load(&p->mode);
    return sequence==atomic_load(&p->sequence) && (mode==PTT_ACTIVE || mode==PTT_DRAINING || (!sending && mode==PTT_ARMED));
}
bool online_ptt_commit_ready(const online_ptt_control_t *p,unsigned sequence,bool empty) {
    return empty && atomic_load(&p->mode)==PTT_DRAINING && sequence==atomic_load(&p->sequence) &&
        atomic_load(&p->ack)==sequence && atomic_load(&p->capture_stopped);
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
    if(!strcmp(code,"passport-speech-too-short"))return "说话太短，请再说一次";
    if(!strcmp(code,"passport-no-speech"))return "没有听清，请再说一次";
    if(!strcmp(code,"passport-no-reply"))return "本轮没有可朗读的回复";
    if(!strcmp(code,"passport-turn-failed"))return "对话失败，请重新提问";
    if(!strcmp(code,"passport-reply-too-long"))return "回复超出时长，请提问短一些";
    if(!strcmp(code,"passport-playback-failed"))return "播音中断，请重新提问";
    if(!strcmp(code,"passport-asr-key-required"))return "请在 DSH 配置识别密钥";
    if(!strcmp(code,"passport-asr-auth-failed"))return "识别密钥无效，请检查 DSH";
    if(!strcmp(code,"passport-asr-permission-denied"))return "请先开通所选语音识别服务";
    if(!strcmp(code,"passport-asr-quota-exceeded"))return "识别额度不足，请检查账户";
    if(!strcmp(code,"passport-asr-rate-limited"))return "识别请求过多，请稍后再试";
    if(!strcmp(code,"passport-asr-timeout"))return "识别超时，请重新说话";
    if(!strcmp(code,"passport-asr-network-failed"))return "识别网络异常，请重试";
    if(!strcmp(code,"passport-recording-too-long"))return "录音太长，请分开说";
    return "后端错误，请检查服务配置";
}

bool online_error_recoverable(const char *code) {
    static const char *const codes[]={"passport-recognition-failed","passport-speech-failed","passport-request-timeout",
        "passport-no-speech","passport-no-reply","passport-turn-failed","passport-reply-too-long",
        "passport-playback-failed","passport-speech-too-short","passport-recording-too-long"};
    if(!code)return false;
    if(!strncmp(code,"passport-asr-",13) && strcmp(code,"passport-asr-unavailable"))return true;
    for(unsigned i=0;i<sizeof(codes)/sizeof(codes[0]);i++)if(!strcmp(code,codes[i]))return true;
    return false;
}
bool online_feedback_progress(online_feedback_t *f,const char *turn,const char *phase,uint64_t now) {
    if(!turn || !*turn || strlen(turn)>=sizeof(f->turn))return false;
    const char *label=NULL;
    if(!strcmp(phase,"capturing"))label="正在收音";
    if(!strcmp(phase,"recognizing"))label="正在识别";
    if(!strcmp(phase,"thinking"))label="正在思考";
    if(!strcmp(phase,"tool"))label="正在执行工具";
    if(!strcmp(phase,"synthesizing"))label="正在合成语音";
    if(!strcmp(phase,"playing"))label="正在回答";
    if(!label)return false;
    if(!strcmp(f->turn,turn) && f->settled)return false;
    if(f->turn[0] && !f->settled && strcmp(f->turn,turn))return false;
    bool waiting=strcmp(phase,"capturing") && strcmp(phase,"playing");
    if(strcmp(f->turn,turn) || (waiting && !f->waiting))f->since=now;
    snprintf(f->turn,sizeof(f->turn),"%s",turn);f->phase=label;f->problem=NULL;f->settled=false;
    f->waiting=waiting;
    return true;
}
bool online_feedback_result(online_feedback_t *f,const char *turn,const char *result,const char *code) {
    if(!turn || !*turn || strlen(turn)>=sizeof(f->turn))return false;
    if(!strcmp(f->turn,turn) && f->settled)return false;
    if(f->turn[0] && !f->settled && strcmp(f->turn,turn))return false;
    if(strcmp(result,"failed") && strcmp(result,"completed") && strcmp(result,"cancelled"))return false;
    snprintf(f->turn,sizeof(f->turn),"%s",turn);f->phase=NULL;f->settled=true;
    f->problem=!strcmp(result,"failed")?online_error_text(code):NULL;
    return true;
}
void online_feedback_text(const online_feedback_t *f,uint64_t now,char *out,size_t size) {
    const char *text=f->problem?f->problem:"";
    if(!f->problem && f->phase && f->waiting && now>=f->since && now-f->since>=8000 && now-f->since<11000)
        text="耗时较长，按下键可取消";
    snprintf(out,size,"%s",text);
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
    return stored<ONLINE_THEME_COUNT?(online_theme_t)stored:ONLINE_THEME_WHALE;
}
online_background_t online_background_normalize(unsigned stored) {
    return stored<ONLINE_BACKGROUND_COUNT?(online_background_t)stored:ONLINE_BACKGROUND_DAY;
}
online_background_t online_background_next(online_background_t current,bool has_custom) {
    static const online_background_t next[ONLINE_BACKGROUND_COUNT]={
        [ONLINE_BACKGROUND_DAY]=ONLINE_BACKGROUND_NIGHT,
        [ONLINE_BACKGROUND_NIGHT]=ONLINE_BACKGROUND_SPRING,
        [ONLINE_BACKGROUND_SPRING]=ONLINE_BACKGROUND_SUMMER,
        [ONLINE_BACKGROUND_SUMMER]=ONLINE_BACKGROUND_AUTUMN,
        [ONLINE_BACKGROUND_AUTUMN]=ONLINE_BACKGROUND_WINTER,
        [ONLINE_BACKGROUND_WINTER]=ONLINE_BACKGROUND_CUSTOM,
        [ONLINE_BACKGROUND_CUSTOM]=ONLINE_BACKGROUND_OFF,
        [ONLINE_BACKGROUND_OFF]=ONLINE_BACKGROUND_DAY,
    };
    online_background_t choice=next[online_background_normalize(current)];
    return choice==ONLINE_BACKGROUND_CUSTOM && !has_custom?ONLINE_BACKGROUND_OFF:choice;
}
static const unsigned volume_steps[]={0,25,50,75,90,100};
unsigned online_volume_next(unsigned current) {
    for(unsigned i=0;i<sizeof(volume_steps)/sizeof(volume_steps[0]);i++)
        if(volume_steps[i]>current)return volume_steps[i];
    return 0;
}
unsigned online_volume_normalize(unsigned stored) {
    for(unsigned i=0;i<sizeof(volume_steps)/sizeof(volume_steps[0]);i++)
        if(volume_steps[i]==stored)return stored;
    return ONLINE_VOLUME_DEFAULT;
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
bool online_standby_busy(online_phase_t phase,unsigned mode,bool reply_pending,
                         bool host_busy,bool capturing,bool feedback_pending) {
    return phase==ONLINE_SETUP || phase==ONLINE_THINKING || phase==ONLINE_SPEAKING ||
        mode==PTT_ARMED || mode==PTT_ACTIVE || mode==PTT_DRAINING ||
        reply_pending || host_busy || capturing || feedback_pending;
}
online_theme_t online_theme_next(online_theme_t theme) {
    return (online_theme_t)((online_theme_normalize(theme)+1)%ONLINE_THEME_COUNT);
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

unsigned online_menu_items(const online_menu_t *menu) {
    switch(menu->page) {
        case ONLINE_SETTINGS: return ONLINE_MENU_COUNT;
        case ONLINE_DISPLAY: return ONLINE_DISPLAY_COUNT;
        case ONLINE_SOUND: return ONLINE_SOUND_COUNT;
        case ONLINE_CONNECTION: return menu->network_count+2;
        default: return 0;
    }
}

static online_action_t resume_network(const online_menu_t *menu) {
    return menu->provisioning && menu->network_count?ONLINE_ACTION_RESUME:ONLINE_ACTION_NONE;
}
static online_action_t return_home(online_menu_t *menu) {
    menu->page=ONLINE_HOME;
    menu->selected=0;
    return resume_network(menu);
}

online_action_t online_menu_key(online_menu_t *m,online_key_t key) {
    if(key==ONLINE_KEY_UP_LONG || key==ONLINE_KEY_DOWN_LONG) {
        if(m->page==ONLINE_SETTINGS) {
            unsigned offset=key==ONLINE_KEY_DOWN_LONG?ONLINE_MENU_COLUMNS:ONLINE_MENU_COUNT-ONLINE_MENU_COLUMNS;
            m->selected=(m->selected+offset)%ONLINE_MENU_COUNT;
            return ONLINE_ACTION_NONE;
        }
        return key==ONLINE_KEY_UP_LONG?ONLINE_ACTION_OVERLAY:ONLINE_ACTION_NONE;
    }
    if(key==ONLINE_KEY_LONG) {
        if(m->page!=ONLINE_HOME)return return_home(m);
        m->page=ONLINE_SETTINGS;m->selected=0;
        return ONLINE_ACTION_NONE;
    }
    if(m->page==ONLINE_HOME) {
        if(key==ONLINE_KEY_OK)return m->provisioning?resume_network(m):ONLINE_ACTION_MIC;
        if(key==ONLINE_KEY_UP)return ONLINE_ACTION_VOLUME;
        if(key==ONLINE_KEY_DOWN)return ONLINE_ACTION_CANCEL;
        return ONLINE_ACTION_NONE;
    }
    unsigned count=online_menu_items(m);
    if(count && (key==ONLINE_KEY_UP || key==ONLINE_KEY_DOWN)) {
        m->selected=(m->selected+(key==ONLINE_KEY_DOWN?1:count-1))%count;
        return ONLINE_ACTION_NONE;
    }
    if(key!=ONLINE_KEY_OK)return ONLINE_ACTION_NONE;
    switch(m->page) {
        case ONLINE_SETTINGS: {
            static const online_page_t pages[]={
                [ONLINE_MENU_DISPLAY]=ONLINE_DISPLAY,[ONLINE_MENU_STATUS]=ONLINE_STATUS,
                [ONLINE_MENU_SOUND]=ONLINE_SOUND,[ONLINE_MENU_UPLOAD]=ONLINE_HOME,
                [ONLINE_MENU_HOME]=ONLINE_HOME,[ONLINE_MENU_CONNECTION]=ONLINE_CONNECTION,
            };
            if(m->selected>=ONLINE_MENU_COUNT)break;
            if(m->selected==ONLINE_MENU_HOME)return return_home(m);
            bool setup=m->selected==ONLINE_MENU_UPLOAD;
            m->page=pages[m->selected];
            m->selected=0;
            return setup?ONLINE_ACTION_SETUP:ONLINE_ACTION_NONE;
        }
        case ONLINE_DISPLAY:
            if(m->selected==ONLINE_DISPLAY_THEME)return ONLINE_ACTION_THEME;
            if(m->selected==ONLINE_DISPLAY_BACKGROUND)return ONLINE_ACTION_BACKGROUND;
            if(m->selected==ONLINE_DISPLAY_BRIGHTNESS)return ONLINE_ACTION_BRIGHTNESS;
            if(m->selected==ONLINE_DISPLAY_IDLE)return ONLINE_ACTION_IDLE;
            if(m->selected==ONLINE_DISPLAY_OVERLAY)return ONLINE_ACTION_OVERLAY;
            m->page=ONLINE_SETTINGS;m->selected=ONLINE_MENU_DISPLAY;
            break;
        case ONLINE_STATUS:
            m->page=ONLINE_SETTINGS;m->selected=ONLINE_MENU_STATUS;
            break;
        case ONLINE_SOUND:
            if(m->selected==ONLINE_SOUND_VOLUME)return ONLINE_ACTION_VOLUME;
            if(m->selected==ONLINE_SOUND_CUE)return ONLINE_ACTION_CUE;
            m->page=ONLINE_SETTINGS;m->selected=ONLINE_MENU_SOUND;
            break;
        case ONLINE_CONNECTION:
            if(m->selected<m->network_count)return m->selected?ONLINE_ACTION_NETWORK:resume_network(m);
            if(m->selected==m->network_count) {
                m->page=ONLINE_HOME;m->selected=0;
                return ONLINE_ACTION_SETUP;
            }
            m->page=ONLINE_SETTINGS;m->selected=ONLINE_MENU_CONNECTION;
            break;
        default: break;
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
