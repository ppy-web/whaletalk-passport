#include "online_runtime.h"
#include "online_setup.h"
#include "online_theme.h"
#include "online_background.h"
#include "online_cue.h"
#include "online_input.h"
#include "online_ui.h"
#include "bsp_audio.h"
#include "bsp_display.h"
#include "esp_websocket_client.h"
#include "esp_crt_bundle.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_mac.h"
#include "esp_random.h"
#include "esp_system.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "esp_sntp.h"
#include <time.h>
#include "nvs_flash.h"
#include "nvs.h"
#include "cJSON.h"
#include "mbedtls/base64.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define PCM_BYTES 640
#define FRAME_MAX 16384
/* Fixed queue budget ~30 KB, one bounded JSON frame <16 KB. */
typedef struct { unsigned epoch,rate; size_t len; bool done,ptt; char id[129]; uint8_t pcm[PCM_BYTES]; } playback_t;
typedef struct { size_t len; unsigned epoch; uint8_t pcm[PCM_BYTES]; } capture_t;
typedef struct { char type[32],id[129]; } receipt_t;
static QueueHandle_t playback,capture,receipts;
/* Playback and push-to-talk are mutually exclusive and reuse the 32-frame queue. */
static online_ptt_control_t ptt;
#define ptt_mode ptt.mode
#define ptt_sequence ptt.sequence
#define ptt_ack ptt.ack
#define ptt_problem ptt.problem
#define ptt_supported ptt.supported
#define ptt_reply ptt.reply
#define ptt_capture_stopped ptt.capture_stopped
#define ptt_audio_started ptt.audio_started
static portMUX_TYPE state_lock=portMUX_INITIALIZER_UNLOCKED;
static online_status_t status={.phase=ONLINE_CONNECTING};
static atomic_bool mic,ready,connected,got_ip,hello_pending,cancel_pending,setup_pending,fault;
static atomic_uint epoch,level,volume=ONLINE_VOLUME_DEFAULT,capture_drops;
static atomic_uint volume_revision;
static atomic_bool volume_save_failed;
static atomic_uint profile_pending;
static atomic_bool profile_save_failed;
static atomic_uint selected_theme=ONLINE_THEME_WHALE,theme_revision;
static atomic_bool theme_save_failed;
static atomic_bool theme_storage_available;
static atomic_uint selected_idle=ONLINE_IDLE_1_MIN,idle_revision;
static atomic_bool idle_save_failed;
static atomic_uint selected_background=ONLINE_BACKGROUND_DAY,background_revision;
static atomic_bool background_save_failed;
static atomic_uint selected_brightness=100,brightness_revision;
static atomic_bool brightness_save_failed;
static atomic_bool overlay=true,overlay_save_failed;
static atomic_uint overlay_revision;
static atomic_bool cues_enabled=true,cues_save_failed,ptt_wait_cue,ptt_long_gesture;
static atomic_uint cues_revision;
static atomic_bool suppress_audio;
static atomic_bool standby_requested;
static _Atomic(TaskHandle_t) audio_worker,maintenance_worker,network_worker;
static esp_websocket_client_handle_t gateway_socket;
static char *frame;
static size_t frame_used,frame_base,chunk_used,chunk_total;
static bool message_active;
static online_config_t config;
static uint32_t serial;
static bool audio_available;
/* Only the websocket event task mutates negotiation. Reconnects start afresh. */
static bool negotiated;
static atomic_bool host_busy,host_capturing;
static char response_id[129];
static int connection_http,connection_errno;
static const char *backend_problem;
static online_feedback_t feedback;
static bool runtime_active(void) {
    portENTER_CRITICAL(&state_lock);
    online_phase_t phase=status.phase;
    bool pending=feedback.phase && !feedback.settled;
    portEXIT_CRITICAL(&state_lock);
    unsigned mode=atomic_load(&ptt_mode);
    return online_standby_busy(phase,mode,atomic_load(&ptt_reply),atomic_load(&host_busy),
                               atomic_load(&host_capturing),pending);
}
static bool microphone_enabled(void) {
    return atomic_load(&mic) && !atomic_load(&standby_requested);
}
bool online_set_standby(bool asleep) {
    if(asleep && (!atomic_load(&theme_storage_available) || runtime_active()))return false;
    bool changed=atomic_exchange(&standby_requested,asleep)!=asleep;
    if(!asleep && changed)online_ui_activity();
    if(changed) {
        // Invalidate both pre-sleep and pre-wake captured frames. The audio task
        // owns codec transitions, so no I2S call can race a PCM read/write.
        atomic_fetch_add(&epoch,1);
        TaskHandle_t task=atomic_load(&audio_worker);if(task)xTaskNotifyGive(task);
        task=atomic_load(&maintenance_worker);if(task)xTaskNotifyGive(task);
        task=atomic_load(&network_worker);if(task)xTaskNotifyGive(task);
    }
    // A turn may have started between the guard and the atomic exchange.
    if(asleep && runtime_active()) {online_set_standby(false);return false;}
    return true;
}
static portMUX_TYPE cue_lock=portMUX_INITIALIZER_UNLOCKED;
static online_cue_queue_t cue_queue;
static uint64_t cue_connection_at[2];
static void cue_clear(void) {portENTER_CRITICAL(&cue_lock);online_cue_clear(&cue_queue);portEXIT_CRITICAL(&cue_lock);}
static void cue_request(online_cue_t kind) {
    if(atomic_load(&standby_requested) || !audio_available || !atomic_load(&cues_enabled) || !atomic_load(&volume))return;
    uint64_t now=(uint64_t)(esp_timer_get_time()/1000);
    portENTER_CRITICAL(&cue_lock);
    if(kind==CUE_CONNECTED || kind==CUE_DISCONNECTED) {
        unsigned index=kind==CUE_CONNECTED?0:1;
        if(cue_connection_at[index] && now+1-cue_connection_at[index]<10000) {portEXIT_CRITICAL(&cue_lock);return;}
        cue_connection_at[index]=now+1;
    }
    online_cue_push(&cue_queue,kind,now);
    portEXIT_CRITICAL(&cue_lock);
}
static bool cue_next(online_cue_t *kind) {
    uint64_t now=(uint64_t)(esp_timer_get_time()/1000);
    portENTER_CRITICAL(&cue_lock);bool found=online_cue_pop(&cue_queue,now,kind);portEXIT_CRITICAL(&cue_lock);return found;
}
static void state(online_phase_t p,const char *message) {
    portENTER_CRITICAL(&state_lock);status.phase=p;
    if(p==ONLINE_ERROR || p==ONLINE_CONNECTING)memset(&feedback,0,sizeof(feedback));
    if(message) snprintf(status.message,sizeof(status.message),"%s",message);
    portEXIT_CRITICAL(&state_lock);
    if(p==ONLINE_SETUP || p==ONLINE_THINKING || p==ONLINE_SPEAKING)online_set_standby(false);
}
void online_snapshot(online_status_t *out) {
    portENTER_CRITICAL(&state_lock);*out=status;
    char feedback_text[sizeof(out->message)];
    online_feedback_text(&feedback,(uint64_t)(esp_timer_get_time()/1000),feedback_text,sizeof(feedback_text));
    /* Progress labels point to immutable strings owned by online_state.c. */
    out->progress=feedback.phase;
    out->feedback_visible=feedback_text[0]!='\0';
    if(out->feedback_visible)snprintf(out->message,sizeof(out->message),"%s",feedback_text);
    portEXIT_CRITICAL(&state_lock);
    unsigned mode=atomic_load(&ptt_mode),problem=atomic_load(&ptt_problem);
    out->ptt_recording=mode==PTT_ACTIVE;
    out->keep_awake=runtime_active() || !atomic_load(&theme_storage_available);
    if(mode==PTT_ACTIVE || mode==PTT_DRAINING || problem) {
        const char *message=mode==PTT_DRAINING?"正在发送语音":mode==PTT_ACTIVE?
            (atomic_load(&ptt_audio_started)?"正在录音，松开发送":"准备录音，请继续按住"):
            problem==1?"连接器不支持对讲":problem==2?"对讲不可用，请检查连接":
            problem==3?"录音拥塞，请重新说话":"没有听清，请再说一次";
        snprintf(out->message,sizeof(out->message),"%s",message);out->feedback_visible=true;
    }
    out->mic=atomic_load(&mic);out->volume=atomic_load(&volume);out->level=atomic_load(&level);
    out->linked=atomic_load(&connected) && atomic_load(&ready);
    out->has_ip=atomic_load(&got_ip);
    out->audio_ok=audio_available;
    out->theme=online_theme_normalize(atomic_load(&selected_theme));out->theme_save_failed=atomic_load(&theme_save_failed);
    out->idle=online_idle_normalize(atomic_load(&selected_idle));out->idle_save_failed=atomic_load(&idle_save_failed);
    out->background=online_background_normalize(atomic_load(&selected_background));
    out->custom_background=online_background_custom_available();
    out->background_save_failed=atomic_load(&background_save_failed);
    out->brightness=online_brightness_normalize(atomic_load(&selected_brightness));
    out->brightness_save_failed=atomic_load(&brightness_save_failed);
    out->overlay=atomic_load(&overlay);out->overlay_save_failed=atomic_load(&overlay_save_failed);
    out->cues=atomic_load(&cues_enabled);out->cues_save_failed=atomic_load(&cues_save_failed);
    out->volume_save_failed=atomic_load(&volume_save_failed);
    out->profile_save_failed=atomic_load(&profile_save_failed);
}
void online_toggle_mic(void) { if(audio_available) {bool next=!atomic_load(&mic);atomic_store(&mic,next);atomic_fetch_add(&epoch,1);if(!next)online_cancel();cue_request(next?CUE_MIC_ON:CUE_MIC_OFF);} }
void online_cancel(void) {
    portENTER_CRITICAL(&state_lock);feedback.problem=NULL;feedback.phase=NULL;feedback.settled=true;portEXIT_CRITICAL(&state_lock);
    cue_clear();atomic_store(&host_capturing,false);atomic_store(&suppress_audio,true);atomic_store(&ptt_reply,false);atomic_store(&ptt_problem,0);atomic_fetch_add(&epoch,1);atomic_store(&cancel_pending,true);
}
void online_ptt_press(void) {
    online_cancel();
    atomic_store(&ptt_long_gesture,false);
    if(!playback || !audio_available || !atomic_load(&ready) || !atomic_load(&ptt_supported))return;
    atomic_store(&ptt_mode,PTT_FAILED);
    xQueueReset(playback);
    atomic_store(&ptt_wait_cue,atomic_load(&cues_enabled) && atomic_load(&volume)>0);
    online_ptt_arm_state(&ptt);
}
void online_ptt_start(void) {
    atomic_store(&ptt_long_gesture,true);
    online_ptt_start_state(&ptt,atomic_load(&ready));
    if(atomic_load(&ptt_mode)!=PTT_ACTIVE)cue_request(CUE_ERROR);
}
void online_ptt_release(void) {
    bool held=atomic_exchange(&ptt_long_gesture,false);
    if(!held)cue_request(CUE_CANCEL);
    else if(online_ptt_release_waiting(&ptt,atomic_load(&ptt_wait_cue))) {
        online_cancel();return;
    }
    else if(atomic_load(&ptt_mode)==PTT_FAILED)cue_request(CUE_ERROR);
    online_ptt_release_state(&ptt);
}
void online_ptt_abort(void) {
    if(atomic_exchange(&ptt_mode,PTT_IDLE)!=PTT_IDLE)online_cancel();
}
void online_volume(void) {
    unsigned current=atomic_load(&volume);
    while(!atomic_compare_exchange_weak(&volume,&current,online_volume_next(current))) {}
    atomic_store(&volume_save_failed,!atomic_load(&theme_storage_available));
    atomic_fetch_add(&volume_revision,1);
    cue_request(CUE_VOLUME);
}
void online_setup(void) { atomic_store(&setup_pending,true); }
void online_select_profile(unsigned index) {
    if(index>=ONLINE_PROFILE_LIMIT)return;
    if(!atomic_load(&theme_storage_available)) {atomic_store(&profile_save_failed,true);return;}
    atomic_store(&profile_save_failed,false);
    unsigned expected=0;
    atomic_compare_exchange_strong(&profile_pending,&expected,index+1);
    TaskHandle_t task=atomic_load(&maintenance_worker);if(task)xTaskNotifyGive(task);
}
void online_cycle_theme(void) {
    unsigned current=atomic_load(&selected_theme);
    while(!atomic_compare_exchange_weak(&selected_theme,&current,online_theme_next(online_theme_normalize(current)))) {}
    atomic_store(&theme_save_failed,!atomic_load(&theme_storage_available));atomic_fetch_add(&theme_revision,1);
}
void online_cycle_idle(void) {
    unsigned current=atomic_load(&selected_idle);
    while(!atomic_compare_exchange_weak(&selected_idle,&current,online_idle_next(online_idle_normalize(current)))) {}
    atomic_store(&idle_save_failed,!atomic_load(&theme_storage_available));atomic_fetch_add(&idle_revision,1);
}
void online_cycle_background(void) {
    unsigned current=atomic_load(&selected_background);
    while(!atomic_compare_exchange_weak(&selected_background,&current,
        online_background_next((online_background_t)current,online_background_custom_available()))) {}
    atomic_store(&background_save_failed,!atomic_load(&theme_storage_available));
    atomic_fetch_add(&background_revision,1);
}
void online_cycle_brightness(void) {
    unsigned current=atomic_load(&selected_brightness);
    while(!atomic_compare_exchange_weak(&selected_brightness,&current,online_brightness_next(current))) {}
    atomic_store(&brightness_save_failed,!atomic_load(&theme_storage_available));
    atomic_fetch_add(&brightness_revision,1);
}
void online_toggle_overlay(void) {
    bool value=atomic_load(&overlay);
    while(!atomic_compare_exchange_weak(&overlay,&value,!value)) {}
    atomic_store(&overlay_save_failed,!atomic_load(&theme_storage_available));atomic_fetch_add(&overlay_revision,1);
}
void online_toggle_cues(void) {
    bool value=atomic_load(&cues_enabled);
    while(!atomic_compare_exchange_weak(&cues_enabled,&value,!value)) {}
    atomic_store(&cues_save_failed,!atomic_load(&theme_storage_available));atomic_fetch_add(&cues_revision,1);
    if(value)cue_clear();else cue_request(CUE_VOLUME);
}
static void display_standby_step(bool *suspended) {
    if(atomic_load(&standby_requested)==*suspended)return;
    if(!bsp_lvgl_lock(1000)) {
        if(!*suspended)online_set_standby(false);
        return;
    }
    bool asleep=atomic_load(&standby_requested);
    esp_err_t e=ESP_OK;
    if(asleep && !*suspended) {
        e=bsp_lvgl_pause();
        if(e==ESP_OK) {
            *suspended=true;
            e=bsp_display_sleep();
        }
        if(e!=ESP_OK)online_set_standby(false);
    } else if(!asleep && *suspended) {
        e=bsp_display_wake();
        if(e==ESP_OK)e=bsp_lvgl_resume();
        if(e==ESP_OK)*suspended=false;
    }
    bsp_lvgl_unlock();
    if(e!=ESP_OK)ESP_LOGW("whale_power","display transition failed: %s",esp_err_to_name(e));
    else if(asleep==*suspended)ESP_LOGI("whale_power","panel and LVGL %s",asleep?"suspended":"resumed");
}
static void theme_task(void *arg) {
    (void)arg;unsigned saved_revision=0,saved_idle_revision=0,saved_background_revision=0,saved_brightness_revision=0,saved_overlay_revision=0,saved_cues_revision=0;
    unsigned saved_volume_revision=0;
    atomic_store(&maintenance_worker,xTaskGetCurrentTaskHandle());
    bool display_suspended=false;
    for(;;) {
        display_standby_step(&display_suspended);
        online_status_t snapshot;online_snapshot(&snapshot);
        unsigned profile=atomic_exchange(&profile_pending,0);
        if(profile) {
            /* This worker also survives provisioning mode. No Wi-Fi/HTTP/LVGL
             * call runs in the button callback. Select by the displayed SSID
             * in case a portal save has reordered the stored profiles. */
            if(profile<=snapshot.profile_count && online_profile_select(snapshot.profile_ssids[profile-1]))esp_restart();
            atomic_store(&profile_save_failed,true);
        }
        unsigned revision=atomic_load(&theme_revision),idle_changed=atomic_load(&idle_revision),
            background_changed=atomic_load(&background_revision),brightness_changed=atomic_load(&brightness_revision);
        if(revision!=saved_revision && snapshot.phase!=ONLINE_SPEAKING && snapshot.phase!=ONLINE_THINKING) {
            bool saved=online_theme_save(snapshot.theme);
            atomic_store(&theme_save_failed,!saved);
            if(!saved)ESP_LOGW("whale_theme","theme persistence failed");
            saved_revision=revision;
        }
        if(idle_changed!=saved_idle_revision && snapshot.phase!=ONLINE_SPEAKING && snapshot.phase!=ONLINE_THINKING) {
            bool saved=online_idle_save(snapshot.idle);
            atomic_store(&idle_save_failed,!saved);
            if(!saved)ESP_LOGW("whale_theme","idle preference persistence failed");
            saved_idle_revision=idle_changed;
        }
        if(background_changed!=saved_background_revision && snapshot.phase!=ONLINE_SPEAKING && snapshot.phase!=ONLINE_THINKING) {
            bool ok=online_background_save(snapshot.background);
            atomic_store(&background_save_failed,!ok);
            if(!ok)ESP_LOGW("whale_theme","background preference persistence failed");
            saved_background_revision=background_changed;
        }
        if(brightness_changed!=saved_brightness_revision && snapshot.phase!=ONLINE_SPEAKING && snapshot.phase!=ONLINE_THINKING) {
            bool ok=online_brightness_save(snapshot.brightness);
            atomic_store(&brightness_save_failed,!ok);
            if(!ok)ESP_LOGW("whale_theme","brightness preference persistence failed");
            saved_brightness_revision=brightness_changed;
        }
        unsigned overlay_changed=atomic_load(&overlay_revision);
        if(overlay_changed!=saved_overlay_revision && snapshot.phase!=ONLINE_SPEAKING && snapshot.phase!=ONLINE_THINKING) {
            bool ok=online_overlay_save(snapshot.overlay);
            atomic_store(&overlay_save_failed,!ok);
            if(!ok)ESP_LOGW("whale_theme","overlay preference persistence failed");
            saved_overlay_revision=overlay_changed;
        }
        unsigned cues_changed=atomic_load(&cues_revision);
        if(cues_changed!=saved_cues_revision && snapshot.phase!=ONLINE_SPEAKING && snapshot.phase!=ONLINE_THINKING) {
            bool ok=online_cues_save(snapshot.cues);
            atomic_store(&cues_save_failed,!ok);saved_cues_revision=cues_changed;
            if(!ok)ESP_LOGW("whale_theme","cue preference persistence failed");
        }
        unsigned volume_changed=atomic_load(&volume_revision);
        if(volume_changed!=saved_volume_revision && snapshot.phase!=ONLINE_SPEAKING && snapshot.phase!=ONLINE_THINKING) {
            bool ok=online_volume_save(atomic_load(&volume));
            atomic_store(&volume_save_failed,!ok);saved_volume_revision=volume_changed;
            if(!ok)ESP_LOGW("whale_theme","volume preference persistence failed");
        }
        ulTaskNotifyTake(pdTRUE,pdMS_TO_TICKS(atomic_load(&standby_requested)?1000:150));
    }
}
static void fail(const char *why) {
    ESP_LOGE("bean_network","%s",why);
    atomic_store(&ready,false);atomic_store(&mic,false);atomic_store(&fault,true);atomic_store(&host_capturing,false);
    atomic_store(&ptt_supported,false);atomic_store(&ptt_reply,false);atomic_store(&ptt_mode,PTT_IDLE);
    atomic_fetch_add(&epoch,1);state(ONLINE_ERROR,why);
}
/* A transient transport failure must not change the user's microphone choice.
 * The websocket client owns reconnects after a disconnect event; only request
 * an explicit restart when a send fails while it still reports connected. */
static void recover_transport(void) {
    bool was_connected=atomic_exchange(&connected,false);
    if(atomic_exchange(&ready,false))cue_request(CUE_DISCONNECTED);
    atomic_store(&host_capturing,false);
    atomic_fetch_add(&epoch,1);
    if(was_connected)atomic_store(&fault,true);
    ESP_LOGW("bean_network","recovering transport; mic=%d restart=%d",atomic_load(&mic),was_connected);
    state(ONLINE_CONNECTING,atomic_load(&got_ip)?"后端断开，正在重连":"Wi-Fi 断开，正在重连");
}
static void receipt(const char *type,const char *id) {
    receipt_t r={0};snprintf(r.type,sizeof(r.type),"%s",type);snprintf(r.id,sizeof(r.id),"%s",id);
    if(xQueueSend(receipts,&r,0)!=pdTRUE)fail("回复确认失败，请重新连接");
}
static const char *string(cJSON *j,const char *key) {
    cJSON *v=cJSON_GetObjectItemCaseSensitive(j,key);return cJSON_IsString(v)?v->valuestring:"";
}
static int number(cJSON *j,const char *key) {
    cJSON *v=cJSON_GetObjectItemCaseSensitive(j,key);return cJSON_IsNumber(v)?v->valueint:0;
}
static void receive_message(void) {
    const char *parse_end=NULL;
    cJSON *j=cJSON_ParseWithOpts(frame,&parse_end,true);
    if(!j){ESP_LOGE("bean_network","JSON parse failed bytes=%u offset=%d heap=%lu largest=%u",(unsigned)frame_used,parse_end?(int)(parse_end-frame):-1,(unsigned long)esp_get_free_heap_size(),(unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));fail("后端消息格式错误");return;}
    const char *type=string(j,"type");
    if(!strcmp(type,"session.ready")) {
        cJSON *caps=cJSON_GetObjectItemCaseSensitive(j,"capabilities"),*cap;
        bool input=false,ack=false,has_ptt=false;
        cJSON_ArrayForEach(cap,caps)if(cJSON_IsString(cap)) {
            if(!strcmp(cap->valuestring,"input.audio"))input=true;
            if(!strcmp(cap->valuestring,"playback.receipts"))ack=true;
            if(!strcmp(cap->valuestring,"input.push_to_talk"))has_ptt=true;
        }
        negotiated=online_session_compatible(string(j,"protocol_version"),input,ack);
        atomic_store(&ptt_supported,negotiated && has_ptt);
        if(!negotiated)fail("后端协议版本不兼容");
    } else if(!strcmp(type,"voice.ready")) {
        if(!negotiated || number(j,"inputSampleRate")!=16000 || number(j,"outputSampleRate")!=24000)fail("后端音频格式不支持");
        else {if(!atomic_exchange(&ready,true))cue_request(CUE_CONNECTED);state(atomic_load(&mic)?ONLINE_LISTENING:ONLINE_READY,audio_available?(atomic_load(&mic)?"我在认真听":"确定：开始对话"):"音频不可用，请检查设备");}
    } else if(!strcmp(type,"voice.connection") && !strcmp(string(j,"state"),"unavailable")) {
        /* Gateway owns provider reconnection. Keep this client/session and the
         * user's mic setting when an idle provider connection is recycled. */
        if(atomic_exchange(&ready,false))cue_request(CUE_DISCONNECTED);
        atomic_store(&host_capturing,false);
        atomic_fetch_add(&epoch,1);
        if(*string(j,"message")) {
            atomic_store(&mic,false);state(ONLINE_ERROR,"语音服务不可用，请检查后端");
        } else {
            ESP_LOGI("bean_network","voice provider recovering; retaining gateway session");
            state(atomic_load(&mic)?ONLINE_CONNECTING:ONLINE_READY,
                atomic_load(&mic)?"语音服务恢复中":"按确定开麦");
        }
    }
    else if(!strcmp(type,"voice.ownership") && !strcmp(string(j,"state"),"busy"))fail("后端正被其他客户端使用");
    else if(!strcmp(type,"voice.deactivated"))fail("对话已被其他客户端接管");
    else if(!strcmp(type,"voice.state") && atomic_load(&ready)) {
        const char *v=string(j,"state");
        if(!strcmp(v,"listening")) {
            atomic_store(&host_busy,false);
            atomic_store(&host_capturing,false);
            atomic_store(&suppress_audio,true);
            response_id[0]=0;
            state(atomic_load(&mic)?ONLINE_LISTENING:ONLINE_READY,atomic_load(&mic)?"我在认真听":"按确定开麦");
        }
        if(!strcmp(v,"processing")) {
            atomic_store(&host_busy,true);
            atomic_fetch_add(&epoch,1);
            atomic_store(&suppress_audio,false);
            response_id[0]=0;
            state(ONLINE_THINKING,"正在请教 DSH");
        }
    } else if(!strcmp(type,"voice.ptt.started")) {
        unsigned sequence=atomic_load(&ptt_sequence);
        char expected[16];snprintf(expected,sizeof(expected),"%u",sequence);
        unsigned mode=atomic_load(&ptt_mode);
        if((mode==PTT_ACTIVE || mode==PTT_DRAINING) && !strcmp(string(j,"captureId"),expected))
            atomic_store(&ptt_ack,sequence);
    } else if(!strcmp(type,"voice.ptt.empty")) {
        char expected[16];snprintf(expected,sizeof(expected),"%u",atomic_load(&ptt_sequence));
        if(atomic_load(&ptt_reply) && !strcmp(string(j,"captureId"),expected)) {
            atomic_store(&ptt_reply,false);atomic_store(&ptt_problem,4);cue_clear();cue_request(CUE_ERROR);
        }
    } else if(!strcmp(type,"response.started")) {
        const char *id=string(j,"responseId");
        if(atomic_load(&host_busy) && !atomic_load(&suppress_audio) && (atomic_load(&mic) || atomic_load(&ptt_reply)) &&
           *id && strlen(id)<sizeof(response_id))snprintf(response_id,sizeof(response_id),"%s",id);
    } else if(!strcmp(type,"audio.delta")) {
        const char *id=string(j,"responseId"),*audio=string(j,"audio");int rate=number(j,"sampleRate");
        if((!atomic_load(&mic) && !atomic_load(&ptt_reply)) || !atomic_load(&ready) || !audio_available || atomic_load(&suppress_audio) ||
           !response_id[0] || strcmp(id,response_id)) {cJSON_Delete(j);return;}
        if((rate!=16000 && rate!=24000) || !*id || strlen(id)>128) {cJSON_Delete(j);fail("后端音频格式不支持");return;}
        /* Decode complete base64 quartets into even PCM chunks. The bounded
         * receive frame and JSON tree already hold the encoded audio; allocating
         * another whole decoded frame can exhaust this no-PSRAM board. */
        size_t encoded_len=strlen(audio);
        playback_t p={.rate=(unsigned)rate,.epoch=atomic_load(&epoch)};
        snprintf(p.id,sizeof(p.id),"%s",id);
        for(size_t off=0;off<encoded_len;) {
            size_t chunk=encoded_len-off;
            if(chunk>848)chunk=848; /* 848 base64 chars -> 636 PCM bytes. */
            if(mbedtls_base64_decode(p.pcm,sizeof(p.pcm),&p.len,
                    (const unsigned char *)audio+off,chunk) || (p.len&1)) {
                fail("后端音频解码失败");break;
            }
            if(p.len && xQueueSend(playback,&p,pdMS_TO_TICKS(100))!=pdTRUE) {
                fail("网络音频拥塞，请重试");break;
            }
            off+=chunk;
        }
    } else if(!strcmp(type,"audio.done")) {
        if(atomic_load(&suppress_audio) || !response_id[0] || strcmp(string(j,"responseId"),response_id)) {cJSON_Delete(j);return;}
        playback_t p={.done=true,.epoch=atomic_load(&epoch)};
        snprintf(p.id,sizeof(p.id),"%s",string(j,"responseId"));
        if(xQueueSend(playback,&p,pdMS_TO_TICKS(100))!=pdTRUE)fail("网络音频拥塞，请重试");
    } else if(!strcmp(type,"playback.clear") || !strcmp(type,"response.interrupted")) {
        atomic_store(&suppress_audio,true);
        response_id[0]=0;
        atomic_fetch_add(&epoch,1);
    } else if(!strcmp(type,"voice.progress") && atomic_load(&ready)) {
        portENTER_CRITICAL(&state_lock);
        bool accepted=online_feedback_progress(&feedback,string(j,"turnId"),string(j,"phase"),(uint64_t)(esp_timer_get_time()/1000));
        portEXIT_CRITICAL(&state_lock);
        if(accepted)atomic_store(&host_capturing,!strcmp(string(j,"phase"),"capturing"));
        if(accepted)online_set_standby(false);
        if(accepted && !strcmp(string(j,"phase"),"capturing") && atomic_load(&ptt_mode)==PTT_IDLE)atomic_store(&ptt_problem,0);
    } else if(!strcmp(type,"voice.result") && atomic_load(&ready)) {
        portENTER_CRITICAL(&state_lock);
        bool accepted=online_feedback_result(&feedback,string(j,"turnId"),string(j,"status"),string(j,"code"));
        bool problem=accepted && feedback.problem;
        portEXIT_CRITICAL(&state_lock);
        if(accepted) {
            atomic_store(&host_capturing,false);
            if(problem && atomic_load(&ptt_mode)!=PTT_IDLE && atomic_load(&ptt_ack)==atomic_load(&ptt_sequence))atomic_store(&ptt_mode,PTT_FAILED);
            atomic_store(&ptt_reply,false);
            atomic_store(&host_busy,false);atomic_store(&suppress_audio,true);atomic_fetch_add(&epoch,1);response_id[0]=0;
            if(problem)cue_request(CUE_ERROR);
            state(atomic_load(&mic)?ONLINE_LISTENING:ONLINE_READY,NULL);
        }
    } else if(!strcmp(type,"error")) {
        /* Never display arbitrary server messages: they can include private data. */
        const char *code=string(j,"code");
        if(atomic_load(&ready) && online_error_recoverable(code)) {
            portENTER_CRITICAL(&state_lock);
            feedback.problem=online_error_text(code);feedback.phase=NULL;feedback.settled=true;
            portEXIT_CRITICAL(&state_lock);
            if(atomic_load(&ptt_mode)!=PTT_IDLE)atomic_store(&ptt_mode,PTT_FAILED);
            atomic_store(&ptt_reply,false);
            atomic_store(&host_busy,false);atomic_store(&host_capturing,false);cue_request(CUE_ERROR);atomic_store(&suppress_audio,true);atomic_fetch_add(&epoch,1);
            state(atomic_load(&mic)?ONLINE_LISTENING:ONLINE_READY,NULL);
        } else {backend_problem=online_error_text(code);fail(backend_problem);}
    }
    cJSON_Delete(j);
}
static void websocket_event(void *arg,esp_event_base_t base,int32_t id,void *data) {
    (void)arg;(void)base;
    if(id==WEBSOCKET_EVENT_CONNECTED) {
        connection_http=connection_errno=0;backend_problem=NULL;
        negotiated=false;response_id[0]=0;atomic_store(&host_busy,false);atomic_store(&host_capturing,false);atomic_store(&suppress_audio,true);atomic_store(&ptt_supported,false);
        atomic_store(&connected,true);atomic_store(&hello_pending,true);frame_used=0;
        state(ONLINE_CONNECTING,"正在启动语音服务");
    } else if(id==WEBSOCKET_EVENT_DISCONNECTED || id==WEBSOCKET_EVENT_ERROR || id==WEBSOCKET_EVENT_CLOSED) {
        if(id==WEBSOCKET_EVENT_ERROR && data) {
            const esp_websocket_event_data_t *problem=data;
            connection_http=problem->error_handle.esp_ws_handshake_status_code;
            connection_errno=problem->error_handle.esp_transport_sock_errno;
        }
        negotiated=false;response_id[0]=0;atomic_store(&host_busy,false);atomic_store(&host_capturing,false);atomic_store(&suppress_audio,true);
        atomic_store(&ptt_supported,false);atomic_store(&ptt_reply,false);
        if(atomic_load(&ptt_mode)!=PTT_IDLE)online_ptt_fail_state(&ptt,2);
        atomic_store(&connected,false);
        if(atomic_exchange(&ready,false))cue_request(CUE_DISCONNECTED);
        atomic_store(&hello_pending,false);atomic_fetch_add(&epoch,1);
        frame_used=0;message_active=false;
        ESP_LOGW("bean_network","websocket event=%ld",(long)id);
        state(backend_problem?ONLINE_ERROR:ONLINE_CONNECTING,backend_problem?backend_problem:
            online_connection_error(connection_http,connection_errno,atomic_load(&got_ip)));
    } else if(id==WEBSOCKET_EVENT_DATA) {
        esp_websocket_event_data_t *d=data;
        if(d->op_code==8 && d->data_len>=2) {
            const unsigned char *close_data=(const unsigned char *)d->data_ptr;
            ESP_LOGW("bean_network","server close code=%u",((unsigned)close_data[0]<<8)|close_data[1]);
        }
        if(d->op_code!=1 && d->op_code!=0)return;
        if(!frame)frame=malloc(FRAME_MAX);
        if(!frame)ESP_LOGE("bean_network","receive buffer allocation failed");
        if(!frame || d->payload_offset<0 || d->data_len<0 || d->payload_len<0) {
            fail("后端消息过大或分片无效");return;
        }
        if(d->payload_offset==0) {
            if(d->op_code==1) {
                if(message_active){fail("后端消息分片顺序错误");return;}
                frame_used=0;message_active=true;
            } else if(!message_active || chunk_used!=chunk_total) {
                fail("后端消息分片顺序错误");return;
            }
            frame_base=frame_used;chunk_used=0;chunk_total=d->payload_len;
        }
        if(!message_active || chunk_total!=(size_t)d->payload_len ||
           !online_fragment(&chunk_used,FRAME_MAX-frame_base,d->payload_offset,d->data_len,d->payload_len)) {
            fail("后端消息过大或分片无效");return;
        }
        memcpy(frame+frame_base+d->payload_offset,d->data_ptr,d->data_len);
        frame_used=frame_base+chunk_used;
        if(chunk_used==chunk_total && d->fin) {
            frame[frame_used]=0;receive_message();frame_used=0;message_active=false;
        }
    }
    if(id!=WEBSOCKET_EVENT_DATA) {
        TaskHandle_t task=atomic_load(&network_worker);if(task)xTaskNotifyGive(task);
    }
}
static void wifi_event(void *arg,esp_event_base_t base,int32_t id,void *data) {
    (void)arg;
    if(base==IP_EVENT && id==IP_EVENT_STA_GOT_IP){atomic_store(&got_ip,true);state(ONLINE_CONNECTING,"正在连接后端");}
    if(base==WIFI_EVENT && id==WIFI_EVENT_STA_DISCONNECTED) {
        wifi_event_sta_disconnected_t *event=data;
        ESP_LOGW("bean_network","wifi disconnected reason=%u",event?event->reason:0);
        atomic_store(&got_ip,false);atomic_store(&ready,false);atomic_fetch_add(&epoch,1);
        state(ONLINE_CONNECTING,"Wi-Fi 断开，正在重试");
    }
}
static bool send_json(cJSON *j) {
    if(!j)return false;
    char event_id[48];snprintf(event_id,sizeof(event_id),"bean_%08lx_%lu",(unsigned long)esp_random(),(unsigned long)++serial);
    cJSON_AddStringToObject(j,"event_id",event_id);char *raw=cJSON_PrintUnformatted(j);cJSON_Delete(j);
    if(!raw)return false;
    if(!atomic_load(&connected) || !esp_websocket_client_is_connected(gateway_socket)){free(raw);return false;}
    int length=strlen(raw);int sent=esp_websocket_client_send_text(gateway_socket,raw,length,pdMS_TO_TICKS(3000));free(raw);
    if(sent!=length)ESP_LOGW("bean_network","send failed bytes=%d sent=%d connected=%d heap=%lu",length,sent,atomic_load(&connected),(unsigned long)esp_get_free_heap_size());
    return sent==length;
}
static cJSON *event(const char *type) {cJSON *j=cJSON_CreateObject();if(j)cJSON_AddStringToObject(j,"type",type);return j;}
static bool send_input_control(const char *type,void *context) {(void)context;return send_json(event(type));}
static void send_hello(void) {
    cJSON *j=event("session.hello"),*protocol=cJSON_AddObjectToObject(j,"protocol");
    cJSON_AddStringToObject(protocol,"min","7.0.0");cJSON_AddStringToObject(protocol,"max","7.0.0");
    cJSON *client=cJSON_AddObjectToObject(j,"client");uint8_t mac[6];esp_read_mac(mac,ESP_MAC_WIFI_STA);
    char instance[40];snprintf(instance,sizeof(instance),"bean_%02x%02x%02x%02x%02x%02x",mac[0],mac[1],mac[2],mac[3],mac[4],mac[5]);
    cJSON_AddStringToObject(client,"type","dsh-passport");cJSON_AddStringToObject(client,"version","0.1.0");cJSON_AddStringToObject(client,"instance_id",instance);
    cJSON *caps=cJSON_AddArrayToObject(j,"capabilities");
    cJSON_AddItemToArray(caps,cJSON_CreateString("input.audio"));
    cJSON_AddItemToArray(caps,cJSON_CreateString("playback.receipts"));
    cJSON_AddItemToArray(caps,cJSON_CreateString("voice.feedback"));
    cJSON_AddItemToArray(caps,cJSON_CreateString("input.push_to_talk"));
    cJSON_AddStringToObject(j,"locale","zh-CN");cJSON_AddStringToObject(j,"time_zone","Asia/Shanghai");
    cJSON *connection=cJSON_AddObjectToObject(j,"connection");
    cJSON_AddBoolToObject(connection,"voice_enabled",true);cJSON_AddBoolToObject(connection,"input_enabled",microphone_enabled());
    cJSON_AddBoolToObject(connection,"output_enabled",true);cJSON_AddBoolToObject(connection,"text_only",false);
    if(!send_json(j))fail("握手失败，请检查后端");
}
/* The audio worker alone owns the codec. Abort between 10 ms blocks so speech
 * playback and a released push-to-talk gesture never wait for a whole cue. */
static bool play_cue(online_cue_t kind,unsigned *format,int *applied,bool holding,unsigned sequence,bool *emitted) {
    static int16_t samples[160];
    *emitted=false;
    if(!audio_available || !atomic_load(&cues_enabled) || !atomic_load(&volume))return false;
    if(*format!=16000) {
        if(bsp_audio_set_format(16000,16,1)!=ESP_OK){fail("扬声器不可用");return false;}
        *format=16000;*applied=-1;
    }
    unsigned level=atomic_load(&volume);bsp_audio_set_volume((uint8_t)level);*applied=(int)level;
    /* Six DMA descriptors can retain ~90 ms. Queue 100 ms of silence after
     * the 80 ms tone so its last sample has left TX before capture resumes. */
    for(unsigned block=0;block<18;block++) {
        if(atomic_load(&standby_requested))return false;
        if(block<8)online_cue_render(kind,block,samples);else memset(samples,0,sizeof(samples));
        unsigned mode=atomic_load(&ptt_mode);
        bool active=holding?mode==PTT_ACTIVE:mode!=PTT_IDLE;
        if(!online_cue_play_allowed(kind,atomic_load(&cues_enabled),atomic_load(&volume),holding,active,atomic_load(&host_busy),atomic_load(&host_capturing),atomic_load(&ptt_reply),uxQueueMessagesWaiting(playback)>0))return false;
        if(holding && atomic_load(&ptt_sequence)!=sequence)return false;
        if(bsp_audio_write(samples,sizeof(samples))!=ESP_OK){fail("扬声器不可用");return false;}
        *emitted=true;
    }
    return true;
}
static void audio_task(void *arg) {
    (void)arg;
    atomic_store(&audio_worker,xTaskGetCurrentTaskHandle());
    /* One lifetime worker owns these buffers. Keep codec call-stack headroom. */
    static playback_t p;
    static capture_t c={.len=PCM_BYTES};
    static char active[129];
    static uint8_t silence[PCM_BYTES];
    int applied_volume=-1;
    int64_t last_metrics=0,starved_since=0,prefill_since=0;
    unsigned playback_starves=0,max_starve_ms=0,max_write_ms=0;
    unsigned active_epoch=0,format=0;int64_t quiet_until=0;online_cue_gate_t ptt_gate={0};
    bool sleeping=false;
    for(;;) {
        if(atomic_load(&standby_requested)) {
            if(runtime_active() || active[0]) {online_set_standby(false);continue;}
            if(!sleeping) {
                cue_clear();xQueueReset(capture);
                esp_err_t e=bsp_audio_sleep();
                sleeping=true;format=0;applied_volume=-1;atomic_store(&level,0);
                if(e!=ESP_OK) {
                    ESP_LOGW("whale_power","audio suspend failed: %s",esp_err_to_name(e));
                    online_set_standby(false);continue;
                }
                ESP_LOGI("whale_power","audio suspended; I2S TX/RX stopped");
            }
            // Wake is notified independently of LVGL and its stopped tick.
            ulTaskNotifyTake(pdTRUE,portMAX_DELAY);continue;
        }
        if(sleeping) {
            esp_err_t e=bsp_audio_wake();
            if(e!=ESP_OK) {fail("麦克风不可用");vTaskDelay(pdMS_TO_TICKS(250));continue;}
            sleeping=false;
            xQueueReset(capture);quiet_until=esp_timer_get_time()+60000;
            ESP_LOGI("whale_power","audio resumed");
        }
        if(atomic_load(&ptt_mode)==PTT_DRAINING)atomic_store(&ptt_capture_stopped,true);
        if(esp_timer_get_time()-last_metrics>10000000) {
            last_metrics=esp_timer_get_time();
            ESP_LOGI("bean_audio","stack_free_min=%u heap_free=%lu ready=%d capture_drops=%u largest=%u",
                (unsigned)uxTaskGetStackHighWaterMark(NULL),(unsigned long)esp_get_free_heap_size(),atomic_load(&ready),atomic_load(&capture_drops),(unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
        }
        if(active[0] && (active_epoch!=atomic_load(&epoch) || !atomic_load(&ready))) {
            receipt("playback.cancelled",active);active[0]=0;quiet_until=esp_timer_get_time()+600000;applied_volume=-1;
        }
        if(!active[0] && atomic_load(&ptt_mode)==PTT_IDLE && !uxQueueMessagesWaiting(playback)) {
            online_cue_t cue;
            if(cue_next(&cue)) {
                bool low=cue==CUE_CONNECTED || cue==CUE_DISCONNECTED || cue==CUE_VOLUME;
                if((!low || !atomic_load(&mic)) && (!atomic_load(&host_busy) || cue==CUE_PTT_SEND)) {
                    bool emitted=false,complete=play_cue(cue,&format,&applied_volume,false,0,&emitted);
                    if(emitted) {int64_t tail=esp_timer_get_time()+(complete?60000:600000);if(tail>quiet_until)quiet_until=tail;}
                }
                continue;
            }
        }
        /* Reuse the existing queue for about 300 ms of starting audio. This
         * absorbs provider/network jitter without allocating another buffer. */
        if(!active[0] && atomic_load(&ready) && atomic_load(&ptt_mode)==PTT_IDLE) {
            static playback_t first;
            if(xQueuePeek(playback,&first,0)==pdTRUE && first.epoch==atomic_load(&epoch) && !first.done) {
                if(!prefill_since)prefill_since=esp_timer_get_time();
                if(online_playback_prefill_wait((unsigned)uxQueueMessagesWaiting(playback),
                        (unsigned)((esp_timer_get_time()-prefill_since)/1000))) {
                    vTaskDelay(pdMS_TO_TICKS(5));continue;
                }
            } else prefill_since=0;
        }
        if(atomic_load(&ptt_mode)==PTT_IDLE && xQueueReceive(playback,&p,pdMS_TO_TICKS(5))==pdTRUE) {
            prefill_since=0;
            if(p.ptt || p.epoch!=atomic_load(&epoch) || !atomic_load(&ready))continue;
            if(starved_since) {
                unsigned waited=(unsigned)((esp_timer_get_time()-starved_since)/1000);
                if(waited>=60 && !p.done){playback_starves++;if(waited>max_starve_ms)max_starve_ms=waited;ESP_LOGW("bean_audio","playback queue empty ms=%u buffered=%u",waited,(unsigned)uxQueueMessagesWaiting(playback));}
                starved_since=0;
            }
            if(p.done) {
                ESP_LOGI("bean_audio","playback stats gaps=%u max_gap_ms=%u max_write_ms=%u",playback_starves,max_starve_ms,max_write_ms);
                playback_starves=0;max_starve_ms=0;max_write_ms=0;
                if(active[0] && !strcmp(active,p.id)) {
                    for(unsigned i=0;i<6;i++) {
                        if(active_epoch!=atomic_load(&epoch) || bsp_audio_write(silence,sizeof(silence))!=ESP_OK)break;
                    }
                    receipt(active_epoch==atomic_load(&epoch)?"playback.ended":"playback.cancelled",active);active[0]=0;quiet_until=esp_timer_get_time()+600000;
                    state(atomic_load(&mic)?ONLINE_LISTENING:ONLINE_READY,atomic_load(&mic)?"你说，我在听":"麦克风已关闭");
                }
                continue;
            }
            if(format!=p.rate) {
                if(bsp_audio_set_format(p.rate,16,1)!=ESP_OK){fail("扬声器不可用");continue;}format=p.rate;applied_volume=-1;
            }
            if(strcmp(active,p.id)) {
                if(active[0])receipt("playback.cancelled",active);
                snprintf(active,sizeof(active),"%s",p.id);active_epoch=p.epoch;
                receipt("playback.started",active);state(ONLINE_SPEAKING,"DSH");
            }
            unsigned requested_volume=atomic_load(&volume);
            if(applied_volume!=(int)requested_volume) {
                bsp_audio_set_volume(requested_volume);applied_volume=(int)requested_volume;

            }
            unsigned peak=0;
            for(size_t i=0;i<p.len;i+=2){int16_t s=(int16_t)((unsigned)p.pcm[i]|((unsigned)p.pcm[i+1]<<8));unsigned a=s<0?-(int)s:s;if(a>peak)peak=a;}
            atomic_store(&level,peak);
            int64_t write_start=esp_timer_get_time();
            if(bsp_audio_write(p.pcm,p.len)!=ESP_OK)fail("扬声器播放失败");
            unsigned write_ms=(unsigned)((esp_timer_get_time()-write_start)/1000);if(write_ms>max_write_ms)max_write_ms=write_ms;
            if(write_ms>=80)ESP_LOGW("bean_audio","slow audio write ms=%u buffered=%u",write_ms,(unsigned)uxQueueMessagesWaiting(playback));
            continue;
        }
        if(active[0]){if(!starved_since)starved_since=esp_timer_get_time();continue;}
        starved_since=0;
        atomic_store(&level,0);
        unsigned mode=atomic_load(&ptt_mode);
        if(mode==PTT_ARMED || mode==PTT_ACTIVE) {
            unsigned sequence=atomic_load(&ptt_sequence);
            if(!audio_available || !atomic_load(&ready) || esp_timer_get_time()<=quiet_until) {vTaskDelay(pdMS_TO_TICKS(5));continue;}
            if(atomic_load(&ptt_wait_cue)) {
                if(mode==PTT_ARMED) {vTaskDelay(pdMS_TO_TICKS(5));continue;}
                if(!atomic_load(&cues_enabled) || !atomic_load(&volume))atomic_store(&ptt_wait_cue,false);
                else if(ptt_gate.sequence!=sequence) {
                    bool emitted=false,completed=play_cue(CUE_PTT_START,&format,&applied_volume,true,sequence,&emitted);
                    if(emitted && !completed)quiet_until=esp_timer_get_time()+600000;
                    if(!completed)continue;
                    online_cue_gate_begin(&ptt_gate,sequence,(uint64_t)esp_timer_get_time());
                }
                if(!online_cue_capture_allowed(atomic_load(&ptt_wait_cue),mode==PTT_ACTIVE,ptt_gate.sequence==sequence,online_cue_gate_ready(&ptt_gate,sequence,(uint64_t)esp_timer_get_time()))) {
                    /* Drop the RX backlog and acoustic tail instead of sending
                     * our own tone to the recognizer. RELEASE is checked on the
                     * next iteration before any captured frame can be queued. */
                    if(bsp_audio_read(c.pcm,PCM_BYTES)!=ESP_OK){fail("麦克风读取失败");continue;}
                    online_cue_gate_discard(&ptt_gate);continue;
                }
                if(atomic_load(&ptt_mode)!=PTT_ACTIVE || atomic_load(&ptt_sequence)!=sequence)continue;
            }
            if(format!=16000){if(bsp_audio_set_format(16000,16,1)!=ESP_OK){fail("麦克风不可用");continue;}format=16000;}
            atomic_store(&ptt_audio_started,true);
            if(bsp_audio_read(c.pcm,PCM_BYTES)!=ESP_OK){fail("麦克风读取失败");continue;}
            if(online_ptt_frame_valid(&ptt,sequence,false)) {
                p=(playback_t){.epoch=sequence,.rate=16000,.len=PCM_BYTES,.ptt=true};
                memcpy(p.pcm,c.pcm,PCM_BYTES);
                if(xQueueSend(playback,&p,0)!=pdTRUE)online_ptt_fail_state(&ptt,3);
            }
            continue;
        }
        if(mode!=PTT_IDLE){vTaskDelay(pdMS_TO_TICKS(5));continue;}
        if(!audio_available || !atomic_load(&ready) || !microphone_enabled() || atomic_load(&host_busy)){vTaskDelay(pdMS_TO_TICKS(20));continue;}
        if(format!=16000){if(bsp_audio_set_format(16000,16,1)!=ESP_OK){fail("麦克风不可用");continue;}format=16000;}
        unsigned read_epoch=atomic_load(&epoch);
        if(bsp_audio_read(c.pcm,PCM_BYTES)!=ESP_OK){fail("麦克风读取失败");continue;}
        if(atomic_load(&ptt_mode)==PTT_IDLE && read_epoch==atomic_load(&epoch) && microphone_enabled() && !atomic_load(&host_busy) && esp_timer_get_time()>quiet_until) {
            c.epoch=read_epoch;
            if(xQueueSend(capture,&c,0)!=pdTRUE) {
                /* Prefer current speech to stale queued audio after a network stall.
                 * A transient full queue must not tear down the voice session. */
                static capture_t discarded;
                if(xQueueReceive(capture,&discarded,0)==pdTRUE)atomic_fetch_add(&capture_drops,1);
                if(xQueueSend(capture,&c,0)!=pdTRUE)atomic_fetch_add(&capture_drops,1);
            }
        }
    }
}
static void network_task(void *arg) {
    (void)arg;
    if(nvs_flash_init()!=ESP_OK || esp_netif_init()!=ESP_OK || esp_event_loop_create_default()!=ESP_OK) {
        state(ONLINE_ERROR,"系统初始化失败");vTaskDelete(NULL);return;
    }
    atomic_store(&selected_theme,online_theme_load());
    atomic_store(&selected_idle,online_idle_load());
    atomic_store(&selected_brightness,online_brightness_load());
    atomic_store(&overlay,online_overlay_load());
    atomic_store(&cues_enabled,online_cues_load());
    atomic_store(&volume,online_volume_load());
    online_background_init();
    online_background_t background=online_background_load();
    if(background==ONLINE_BACKGROUND_CUSTOM && !online_background_custom_available()) {
        background=ONLINE_BACKGROUND_DAY;
        ESP_LOGW("whale_theme","custom background unavailable; using built-in background");
    }
    atomic_store(&selected_background,background);
    char saved_ssids[ONLINE_PROFILE_LIMIT][33];unsigned profile_count=0;
    bool profiles_ok=online_profiles_summary(saved_ssids,&profile_count);
    portENTER_CRITICAL(&state_lock);
    status.profile_count=profile_count;memcpy(status.profile_ssids,saved_ssids,sizeof(saved_ssids));
    portEXIT_CRITICAL(&state_lock);
    atomic_store(&profile_save_failed,!profiles_ok);
    bool theme_worker=xTaskCreate(theme_task,"whale_theme",3072,NULL,2,NULL)==pdPASS;
    atomic_store(&theme_storage_available,theme_worker);atomic_store(&theme_save_failed,!theme_worker);
    atomic_store(&idle_save_failed,!theme_worker);
    atomic_store(&overlay_save_failed,!theme_worker);
    atomic_store(&cues_save_failed,!theme_worker);
    if(!online_load_config(&config)) {
        state(ONLINE_CONNECTING,"正在扫描附近 Wi-Fi");
        char info[128];esp_err_t e=online_start_setup(info,sizeof(info));state(e==ESP_OK?ONLINE_SETUP:ONLINE_ERROR,e==ESP_OK?info:"配网启动失败");
        vTaskDelete(NULL);return;
    }
    /* Reserve the largest application allocation before Wi-Fi/TLS fragments
     * the heap. TLS itself uses fixed lifetime buffers for this application. */
    frame=malloc(FRAME_MAX);
    if(!frame){state(ONLINE_ERROR,"内存不足，无法启动");vTaskDelete(NULL);return;}
    state(ONLINE_CONNECTING,"正在连接 Wi-Fi");
    portENTER_CRITICAL(&state_lock);
    status.configured=true;status.token_set=config.token[0]!=0;
    snprintf(status.ssid,sizeof(status.ssid),"%s",config.ssid);
    const char *host=strstr(config.url,"://");host=host?host+3:config.url;
    size_t host_len=strcspn(host,"/");if(host_len>=sizeof(status.host))host_len=sizeof(status.host)-1;
    memcpy(status.host,host,host_len);status.host[host_len]=0;
    portEXIT_CRITICAL(&state_lock);
    esp_netif_create_default_wifi_sta();wifi_init_config_t init=WIFI_INIT_CONFIG_DEFAULT();
    wifi_config_t wifi={0};memcpy(wifi.sta.ssid,config.ssid,strlen(config.ssid));memcpy(wifi.sta.password,config.password,strlen(config.password));
    if(esp_event_handler_register(WIFI_EVENT,ESP_EVENT_ANY_ID,wifi_event,NULL)!=ESP_OK ||
       esp_event_handler_register(IP_EVENT,IP_EVENT_STA_GOT_IP,wifi_event,NULL)!=ESP_OK || esp_wifi_init(&init)!=ESP_OK ||
       esp_wifi_set_storage(WIFI_STORAGE_RAM)!=ESP_OK || esp_wifi_set_mode(WIFI_MODE_STA)!=ESP_OK ||
       esp_wifi_set_config(WIFI_IF_STA,&wifi)!=ESP_OK || esp_wifi_start()!=ESP_OK) {
        state(ONLINE_ERROR,"Wi-Fi 初始化失败");vTaskDelete(NULL);return;
    }
    /* Voice streaming needs prompt uplink delivery after opening the microphone. */
    if(esp_wifi_set_ps(WIFI_PS_NONE)!=ESP_OK)ESP_LOGW("bean_network","Wi-Fi power-save disable failed");
    memset(wifi.sta.password,0,sizeof(wifi.sta.password));memset(config.password,0,sizeof(config.password));
    /* Start only after DHCP, so offline time does not exhaust SNTP retries. */
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    static const char *const time_sources[]={"ntp.aliyun.com","ntp1.aliyun.com","pool.ntp.org"};
    char headers[240]={0};
    if(config.token[0])snprintf(headers,sizeof(headers),"Authorization: Bearer %s\r\n",config.token);
    esp_websocket_client_config_t ws={.uri=config.url,.headers=config.token[0]?headers:NULL,.buffer_size=2048,.task_stack=6144,
        .keep_alive_enable=true,.keep_alive_idle=10,.keep_alive_interval=5,.keep_alive_count=3,
        .ping_interval_sec=10,.pingpong_timeout_sec=120,.network_timeout_ms=8000,.reconnect_timeout_ms=5000,.enable_close_reconnect=true,.crt_bundle_attach=esp_crt_bundle_attach};
    gateway_socket=esp_websocket_client_init(&ws);memset(headers,0,sizeof(headers));memset(config.token,0,sizeof(config.token));
    if(!gateway_socket || esp_websocket_register_events(gateway_socket,WEBSOCKET_EVENT_ANY,websocket_event,NULL)!=ESP_OK) {
        state(ONLINE_ERROR,"后端连接初始化失败");vTaskDelete(NULL);return;
    }
    // Publish only after every early-delete path: notifications must never
    // target a handle belonging to a failed initialization task.
    atomic_store(&network_worker,xTaskGetCurrentTaskHandle());
    online_time_wait_t time_wait={0};
    bool started=false;online_input_control_t input={0};
    wifi_ps_type_t applied_ps=WIFI_PS_NONE;
    int64_t last_connect=0,ptt_started_at=0;unsigned ptt_sent=0;capture_t c;receipt_t r;
    static playback_t ptt_frame;
    for(;;) {
        if(atomic_exchange(&setup_pending,false)) {
            nvs_handle_t n;esp_err_t e=nvs_open("dsh_voice",NVS_READWRITE,&n);
            if(e==ESP_OK){e=nvs_set_u8(n,"setup",1);if(e==ESP_OK)e=nvs_commit(n);nvs_close(n);}
            if(e==ESP_OK)esp_restart();else state(ONLINE_ERROR,"无法进入配置，请重试");
        }
        wifi_ps_type_t desired_ps=atomic_load(&standby_requested)?WIFI_PS_MIN_MODEM:WIFI_PS_NONE;
        if(applied_ps!=desired_ps) {
            esp_err_t e=esp_wifi_set_ps(desired_ps);
            if(e==ESP_OK) {applied_ps=desired_ps;ESP_LOGI("whale_power","Wi-Fi power save %s",desired_ps==WIFI_PS_NONE?"off":"minimum modem");}
            else {ESP_LOGW("whale_power","Wi-Fi power save failed: %s",esp_err_to_name(e));online_set_standby(false);}
        }
        bool has_ip=atomic_load(&got_ip);
        if(!started) {
            bool was_waiting=time_wait.active;
            online_time_action_t action=online_time_step(&time_wait,has_ip,
                !strncmp(config.url,"wss://",6) && time(NULL)<1704067200,
                (uint64_t)(esp_timer_get_time()/1000));
            if(was_waiting && !has_ip)esp_sntp_stop();
            if(action==ONLINE_TIME_START || action==ONLINE_TIME_RETRY) {
                esp_sntp_stop();
                esp_sntp_setservername(0,time_sources[time_wait.source_index]);
                esp_sntp_init();
                ESP_LOGI("bean_network","time sync %s source=%u",action==ONLINE_TIME_START?"started":"retry",time_wait.source_index);
            }
            if(was_waiting && !time_wait.active && has_ip)ESP_LOGI("bean_network","time sync complete");
        }
        if(!has_ip) {
            if(esp_timer_get_time()-last_connect>5000000 || !last_connect){esp_wifi_connect();last_connect=esp_timer_get_time();}
            vTaskDelay(pdMS_TO_TICKS(50));continue;
        }
        if(time_wait.active) {
            state(ONLINE_CONNECTING,time_wait.timed_out?"校时超时，正在重试":"正在同步时间");
            vTaskDelay(pdMS_TO_TICKS(100));continue;
        }
        if(!started){state(ONLINE_CONNECTING,"正在连接后端");esp_websocket_client_start(gateway_socket);started=true;}
        if(!atomic_load(&fault) && atomic_load(&connected) && atomic_exchange(&hello_pending,false)) {send_hello();input.synced=false;}
        if(atomic_exchange(&fault,false)) {
            esp_websocket_client_stop(gateway_socket);atomic_store(&connected,false);vTaskDelay(pdMS_TO_TICKS(1000));
            esp_websocket_client_start(gateway_socket);continue;
        }
        if(atomic_exchange(&cancel_pending,false)) {
            atomic_fetch_add(&epoch,1);
            if(atomic_load(&connected))send_json(event("response.cancel"));
            if(atomic_load(&ready))state(atomic_load(&mic)?ONLINE_LISTENING:ONLINE_READY,atomic_load(&mic)?"你说，我在听":"麦克风已关闭");
        }
        bool enabled=microphone_enabled();
        if(atomic_load(&connected) && (!input.synced || input.enabled!=enabled)) {
            /* Clear upstream buffered input on pause; wake the retained session
             * before resuming. These controls must precede fresh audio frames. */
            if(!online_input_sync(&input,enabled,send_input_control,NULL)) {recover_transport();continue;}
            ESP_LOGI("bean_network","microphone %s",enabled?"resumed":"paused");
            if(atomic_load(&ready))state(enabled?ONLINE_LISTENING:ONLINE_READY,enabled?"你说，我在听":"麦克风已关闭");
            else if(enabled && atomic_load(&connected))state(ONLINE_CONNECTING,"语音服务恢复中");
        }
        while(xQueueReceive(receipts,&r,0)==pdTRUE)if(atomic_load(&connected)) {
            cJSON *j=event(r.type);cJSON_AddStringToObject(j,"responseId",r.id);if(!send_json(j)){recover_transport();break;}
        }
        unsigned mode=atomic_load(&ptt_mode),sequence=atomic_load(&ptt_sequence);
        if(!atomic_load(&connected) || !atomic_load(&ready)) {
            ptt_sent=0;
            if(mode==PTT_ARMED || mode==PTT_ACTIVE || mode==PTT_DRAINING) {
                online_ptt_fail_state(&ptt,2);
            }
        } else if(mode==PTT_ACTIVE || mode==PTT_DRAINING) {
            if(!online_cue_upload_allowed(atomic_load(&ptt_wait_cue),atomic_load(&ptt_audio_started))) {vTaskDelay(pdMS_TO_TICKS(5));continue;}
            if(ptt_sent!=sequence) {
                char id[16];snprintf(id,sizeof(id),"%u",sequence);
                cJSON *j=event("input_audio_buffer.start");cJSON_AddStringToObject(j,"captureId",id);
                if(!send_json(j)){recover_transport();continue;}
                ptt_sent=sequence;ptt_started_at=esp_timer_get_time();
            }
            if(atomic_load(&ptt_ack)!=sequence && esp_timer_get_time()-ptt_started_at>3000000) {
                online_ptt_fail_state(&ptt,2);continue;
            }
            if(xQueueReceive(playback,&ptt_frame,0)==pdTRUE) {
                if(ptt_frame.ptt && ptt_frame.epoch==sequence && online_ptt_frame_valid(&ptt,sequence,true)) {
                    unsigned char encoded[PCM_BYTES*4/3+8];size_t len=0;
                    if(!mbedtls_base64_encode(encoded,sizeof(encoded),&len,ptt_frame.pcm,ptt_frame.len)) {
                        encoded[len]=0;cJSON *j=event("input_audio_buffer.append");cJSON_AddStringToObject(j,"audio",(char *)encoded);
                        if(!send_json(j)){recover_transport();continue;}
                    } else online_ptt_fail_state(&ptt,3);
                }
            } else if(online_ptt_commit_ready(&ptt,sequence,true)) {
                char id[16];snprintf(id,sizeof(id),"%u",sequence);
                cJSON *j=event("input_audio_buffer.commit");cJSON_AddStringToObject(j,"captureId",id);
                atomic_store(&ptt_reply,true);
                if(!send_json(j)){atomic_store(&ptt_reply,false);recover_transport();continue;}
                atomic_store(&ptt_mode,PTT_IDLE);ptt_sent=0;cue_request(CUE_PTT_SEND);
            }
            vTaskDelay(pdMS_TO_TICKS(1));continue;
        } else if(mode==PTT_ARMED || mode==PTT_FAILED) {
            if(mode==PTT_FAILED && ptt_sent) {send_json(event("response.cancel"));ptt_sent=0;}
            vTaskDelay(pdMS_TO_TICKS(5));continue;
        } else if(ptt_sent) {
            send_json(event("response.cancel"));ptt_sent=0;
        }
        if(atomic_load(&standby_requested)) {
            xQueueReset(capture);ulTaskNotifyTake(pdTRUE,pdMS_TO_TICKS(1000));continue;
        }
        if(xQueueReceive(capture,&c,pdMS_TO_TICKS(10))==pdTRUE && atomic_load(&connected) && atomic_load(&ready) && input.synced && input.enabled && microphone_enabled() && !atomic_load(&host_busy) && c.epoch==atomic_load(&epoch)) {
            unsigned char encoded[PCM_BYTES*4/3+8];size_t len=0;
            if(!mbedtls_base64_encode(encoded,sizeof(encoded),&len,c.pcm,c.len)) {
                encoded[len]=0;cJSON *j=event("input_audio_buffer.append");cJSON_AddStringToObject(j,"audio",(char *)encoded);
                if(!send_json(j))recover_transport();
            }
        }
    }
}
void online_start(bool audio_ok) {
    audio_available=audio_ok;
    playback=xQueueCreate(32,sizeof(playback_t));capture=xQueueCreate(8,sizeof(capture_t));receipts=xQueueCreate(8,sizeof(receipt_t));
    if(!playback || !capture || !receipts){state(ONLINE_ERROR,"内存不足，无法启动");return;}
    if(xTaskCreate(network_task,"bean_network",8192,NULL,4,NULL)!=pdPASS ||
       xTaskCreate(audio_task,"bean_audio",8192,NULL,5,NULL)!=pdPASS)state(ONLINE_ERROR,"任务启动失败");
}
