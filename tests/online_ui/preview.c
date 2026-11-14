/* Production LVGL renderer with synthetic status, never a device capture. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "src/misc/lv_text_private.h"
#include "src/draw/lv_image_decoder_private.h"
#include "src/widgets/label/lv_label_private.h"
#include "online_ui.c"
static online_status_t fixture;
static int battery_value=87;
static unsigned toggles,cancels,volumes,setups,themes;
static unsigned ptt_presses,ptt_starts,ptt_releases;
static unsigned idle_changes,brightness_changes;
static unsigned network_switches,selected_network;
static bool standby;
static unsigned standby_entries,wake_requests;
bool online_set_standby(bool asleep) {
    if(asleep && fixture.keep_awake)return false;
    if(asleep && !standby)standby_entries++;
    if(!asleep) {wake_requests++;if(standby)online_ui_activity();}
    standby=asleep;return true;
}
static uint8_t custom_data[ONLINE_BG_BYTES];
const uint8_t *online_background_data(void){return fixture.custom_background?custom_data:NULL;}
void online_cycle_background(void){fixture.background=online_background_next(fixture.background,fixture.custom_background);}
static unsigned backlight=100,backlight_changes;
void bsp_display_backlight(uint8_t percent){backlight=percent;backlight_changes++;}
static int64_t clock_us=2200000;
int64_t esp_timer_get_time(void){return clock_us;}
int bsp_battery_soc(void){return battery_value;}
void online_snapshot(online_status_t *s){*s=fixture;}
void online_toggle_mic(void){toggles++;}
void online_cancel(void){cancels++;}
void online_volume(void){fixture.volume=online_volume_next(fixture.volume);volumes++;}
void online_setup(void){setups++;}
void online_select_profile(unsigned index){network_switches++;selected_network=index;}
void online_cycle_theme(void){fixture.theme=online_theme_next(fixture.theme);themes++;}
void online_cycle_idle(void){fixture.idle=online_idle_next(fixture.idle);idle_changes++;}
void online_cycle_brightness(void){fixture.brightness=online_brightness_next(fixture.brightness);brightness_changes++;}
void online_toggle_overlay(void){fixture.overlay=!fixture.overlay;}
void online_toggle_cues(void){fixture.cues=!fixture.cues;}
void online_ptt_press(void){ptt_presses++;cancels++;}
void online_ptt_start(void){ptt_starts++;fixture.ptt_recording=true;}
void online_ptt_release(void){ptt_releases++;fixture.ptt_recording=false;}
void online_ptt_abort(void){}
static void check_labels(lv_obj_t *o){
    if(!lv_obj_is_visible(o))return;
    if(lv_obj_check_type(o,&lv_label_class)){
        const char *s=lv_label_get_text(o);lv_area_t a;lv_obj_get_coords(o,&a);
        assert(a.x1>=0 && a.y1>=0 && a.x2<240);
        if(!lv_obj_has_flag(lv_obj_get_parent(o),LV_OBJ_FLAG_SCROLLABLE))assert(a.y2<320);
        lv_point_t dim;const lv_font_t *f=lv_obj_get_style_text_font(o,0);
        lv_text_get_size(&dim,s,f,0,lv_obj_get_style_text_line_space(o,0),lv_obj_get_width(o),LV_TEXT_FLAG_NONE);
        if(lv_label_get_long_mode(o)==LV_LABEL_LONG_DOT || lv_label_get_long_mode(o)==LV_LABEL_LONG_SCROLL ||
           lv_label_get_long_mode(o)==LV_LABEL_LONG_SCROLL_CIRCULAR)dim.y=lv_obj_get_height(o);
        if(dim.y>lv_obj_get_height(o))fprintf(stderr,"Clipped: %s %d > %d\n",s,(int)dim.y,(int)lv_obj_get_height(o));
        assert(dim.y<=lv_obj_get_height(o));
        for(uint32_t i=0;s[i];){uint32_t cp=lv_text_encoded_next(s,&i);if(cp=='\n')continue;
            lv_font_glyph_dsc_t d;
            if(!lv_font_get_glyph_dsc(f,&d,cp,0) || d.is_placeholder) {
                fprintf(stderr,"Missing glyph U+%04X in: %s\n",(unsigned)cp,s);
                assert(false);
            }
        }
        lv_obj_t *p=lv_obj_get_parent(o);lv_area_t parent;lv_obj_get_coords(p,&parent);
        if(lv_obj_has_flag(p,LV_OBJ_FLAG_SCROLLABLE)) {
            assert(a.x1>=parent.x1 && a.x2<=parent.x2);
        } else {
            assert(a.x1>=parent.x1 && a.y1>=parent.y1 && a.x2<=parent.x2 && a.y2<=parent.y2);
        }
    }
    for(unsigned i=0;i<lv_obj_get_child_count(o);i++) {
        lv_obj_t *a=lv_obj_get_child(o,i);
        if(lv_obj_has_flag(a,LV_OBJ_FLAG_HIDDEN) || !lv_obj_check_type(a,&lv_label_class))continue;
        lv_area_t x;lv_obj_get_coords(a,&x);
        for(unsigned j=i+1;j<lv_obj_get_child_count(o);j++) {
            lv_obj_t *b=lv_obj_get_child(o,j);
            if(lv_obj_has_flag(b,LV_OBJ_FLAG_HIDDEN) || !lv_obj_check_type(b,&lv_label_class))continue;
            lv_area_t y;lv_obj_get_coords(b,&y);
            bool separate=x.x2<y.x1 || y.x2<x.x1 || x.y2<y.y1 || y.y2<x.y1;
            if(!separate)fprintf(stderr,"Overlapping labels: %s / %s\n",lv_label_get_text(a),lv_label_get_text(b));
            assert(separate);
        }
    }
    for(unsigned i=0;i<lv_obj_get_child_count(o);i++)check_labels(lv_obj_get_child(o,i));
}
static void check(void){lv_obj_update_layout(lv_screen_active());check_labels(lv_screen_active());}
static uint32_t object_render_hash(lv_obj_t *object){
    /* Exercise the actual drawing callback, not just the stored state value. */
    lv_draw_buf_t *b=lv_snapshot_take(object,LV_COLOR_FORMAT_ARGB8888);assert(b);
    uint32_t hash=2166136261u;
    for(uint32_t i=0;i<b->data_size;i++)hash=(hash^b->data[i])*16777619u;
    lv_draw_buf_destroy(b);return hash;
}
static uint32_t icon_render_hash(ui_icon_t *icon){return object_render_hash(icon->obj);}
static void check_icon_flash_sources(void){
    const lv_image_dsc_t *images[]={&ui_link_off_image,&ui_link_on_image,
        &ui_mic_off_image,&ui_mic_on_image,&ui_battery_image,&ui_up_image,&ui_down_image,
        &ui_ok_image,&ui_stop_image,&ui_thinking_0_image,&ui_thinking_1_image,
        &ui_thinking_2_image,&ui_listen_image,&ui_volume_off_image,&ui_volume_low_image,
        &ui_volume_high_image,&ui_volume_level_image};
    for(unsigned i=0;i<sizeof(images)/sizeof(images[0]);i++) {
        lv_image_decoder_dsc_t decoder;
        assert(lv_image_decoder_open(&decoder,images[i],NULL)==LV_RESULT_OK);
        /* Aligned A8 masks are used in place, without a decoded pixel buffer. */
        assert(decoder.decoded && decoder.decoded->data==images[i]->data);
        lv_image_decoder_close(&decoder);
    }
}
static void snap(const char *name){
    check();lv_draw_buf_t *b=lv_snapshot_take(lv_screen_active(),LV_COLOR_FORMAT_RGB888);assert(b);
    char path[100];snprintf(path,sizeof(path),"%s.ppm",name);FILE *f=fopen(path,"wb");assert(f);
    fprintf(f,"P6\n%u %u\n255\n",b->header.w,b->header.h);
    for(unsigned y=0;y<b->header.h;y++)for(unsigned x=0;x<b->header.w;x++){
        uint8_t *p=b->data+y*b->header.stride+x*3;uint8_t rgb[]={p[2],p[1],p[0]};fwrite(rgb,1,3,f);
    }
    fclose(f);lv_draw_buf_destroy(b);
}
int main(void) {
    lv_init();assert(lv_display_create(240,320));fixture.volume=75;fixture.audio_ok=true;fixture.overlay=true;fixture.cues=true;
    check_icon_flash_sources();
    online_ui_enter(true);
    for(unsigned frame=0;frame<ONLINE_AVATAR_COUNT;frame++) {
        lv_image_decoder_dsc_t decoder;
        assert(lv_image_decoder_open(&decoder,whale_avatar_view_source((online_avatar_t)frame),NULL)==LV_RESULT_OK);
        assert(decoder.args.no_cache && decoder.decoded==NULL);
        lv_area_t full={0,0,239,284},area={LV_COORD_MIN,LV_COORD_MIN,LV_COORD_MIN,LV_COORD_MIN};
        unsigned rows=0,alpha_total=0;
        uint8_t reference_row[240*3];
        while(lv_image_decoder_get_area(&decoder,&full,&area)==LV_RESULT_OK) {
            const lv_draw_buf_t *row=decoder.decoded;
            assert(row && row->data_size<=1024);
            for(unsigned n=0;n<240;n++)alpha_total+=row->data[row->header.stride+n];
            if(area.y1==71) {
                memcpy(reference_row,row->data,240*2);
                memcpy(reference_row+240*2,row->data+row->header.stride,240);
            }
            rows++;
        }
        assert(rows==285 && alpha_total>1000);
        lv_image_decoder_close(&decoder);
        assert(lv_image_decoder_open(&decoder,whale_avatar_view_source((online_avatar_t)frame),NULL)==LV_RESULT_OK);
        full=(lv_area_t){5,71,227,71};area=(lv_area_t){LV_COORD_MIN,LV_COORD_MIN,LV_COORD_MIN,LV_COORD_MIN};
        assert(lv_image_decoder_get_area(&decoder,&full,&area)==LV_RESULT_OK);
        const lv_draw_buf_t *row=decoder.decoded;
        for(unsigned n=0;n<223;n++) {
            assert(!memcmp(row->data+n*2,reference_row+(n+5)*2,2));
            assert(row->data[row->header.stride+n]==reference_row[480+n+5]);
        }
        assert(lv_image_decoder_get_area(&decoder,&full,&area)==LV_RESULT_INVALID);
        lv_image_decoder_close(&decoder);
    }
    const char *messages[]={"DSH-Voice-1234","正在连接后端","确定：开始对话","我在认真听","正在请教 Agent","DSH","后端消息过大或分片无效"};
    const char *names[]={"setup","connecting","ready","listening","thinking","speaking","error"};
    for(unsigned theme=0;theme<2;theme++) {
        fixture.theme=online_theme_normalize(theme);
        for(unsigned i=0;i<7;i++){
            fixture.phase=i;fixture.mic=i>=3 && i<=5;fixture.level=19000;
            fixture.linked=i>=ONLINE_READY && i<=ONLINE_SPEAKING;
            snprintf(fixture.message,sizeof(fixture.message),"%s",messages[i]);refresh(NULL);
            char name[48];snprintf(name,sizeof(name),"%s-%s",theme==ONLINE_THEME_WHALE?"whale":"mint",names[i]);snap(name);
            assert(lv_obj_has_flag(whale_layer,LV_OBJ_FLAG_HIDDEN)==(i==ONLINE_SETUP || theme!=ONLINE_THEME_WHALE));
            assert(lv_obj_has_flag(face_layer,LV_OBJ_FLAG_HIDDEN)==(i==ONLINE_SETUP || theme!=ONLINE_THEME_MINT));
            assert(!lv_obj_has_flag(title,LV_OBJ_FLAG_HIDDEN));
            if(i==ONLINE_ERROR || i==ONLINE_CONNECTING)assert(!strcmp(lv_label_get_text(title),messages[i]));
            assert(lv_obj_has_flag(home_controls,LV_OBJ_FLAG_HIDDEN)==(i==ONLINE_SETUP));
        }
    }
    fixture.theme=ONLINE_THEME_WHALE;fixture.phase=ONLINE_CONNECTING;
    const char *connections[]={"正在连接 Wi-Fi","Wi-Fi 断开，正在重试", "正在连接后端","后端断开，正在重连"};
    for(unsigned i=0;i<sizeof(connections)/sizeof(connections[0]);i++) {
        snprintf(fixture.message,sizeof(fixture.message),"%s",connections[i]);refresh(NULL);
        assert(!strcmp(lv_label_get_text(title),connections[i]));
    }
    fixture.phase=ONLINE_READY;fixture.linked=true;fixture.mic=false;refresh(NULL);
    assert(!*lv_label_get_text(title) && link_icon.value);
    fixture.mic=true;refresh(NULL);assert(!*lv_label_get_text(title) && action_mic.value);
    fixture.linked=false;refresh(NULL);assert(strstr(lv_label_get_text(title),"后端"));
    fixture.theme=ONLINE_THEME_WHALE;fixture.phase=ONLINE_LISTENING;fixture.mic=true;fixture.linked=true;fixture.feedback_visible=true;
    snprintf(fixture.message,sizeof(fixture.message),"%s",online_error_text("passport-no-speech"));refresh(NULL);snap("recognition-error");
    assert(!strcmp(lv_label_get_text(title),fixture.message));
    fixture.phase=ONLINE_THINKING;
    snprintf(fixture.message,sizeof(fixture.message),"耗时较长，按下键可取消");refresh(NULL);snap("recognition-slow");
    fixture.feedback_visible=false;
    fixture.phase=ONLINE_LISTENING;
    const char *progress[]={"capturing","recognizing","thinking","tool","synthesizing","playing"};
    online_feedback_t progress_state={0};
    for(unsigned i=0;i<sizeof(progress)/sizeof(progress[0]);i++) {
        assert(online_feedback_progress(&progress_state,"turn",progress[i],i*100));
        fixture.progress=progress_state.phase;refresh(NULL);
        assert(!strcmp(lv_label_get_text(title),progress_state.phase));
        if(i==0)snap("capture-progress");
    }
    fixture.feedback_visible=true;refresh(NULL);
    assert(!strcmp(lv_label_get_text(title),fixture.message));
    fixture.feedback_visible=false;
    assert(online_feedback_result(&progress_state,"turn","completed",""));
    fixture.progress=progress_state.phase;refresh(NULL);
    assert(!fixture.progress && !*lv_label_get_text(title) && action_mic.value);
    fixture.phase=ONLINE_THINKING;
    uint32_t thinking_hashes[3]={0};
    for(unsigned i=0;i<32;i++) {
        clock_us=(int64_t)i*100000;refresh(NULL);
        char name[48];snprintf(name,sizeof(name),"thinking-animation-%02u",i);snap(name);
        assert(fixture.phase==ONLINE_THINKING && backlight==100);
        assert(phase_icon.value>=0 && phase_icon.value<3);
        thinking_hashes[phase_icon.value]=icon_render_hash(&phase_icon);
    }
    assert(thinking_hashes[0] && thinking_hashes[1] && thinking_hashes[2]);
    assert(thinking_hashes[0]!=thinking_hashes[1] && thinking_hashes[0]!=thinking_hashes[2] &&
        thinking_hashes[1]!=thinking_hashes[2]);
    unsigned cancelled_while_thinking=cancels;
    online_ui_key(BSP_BTN_DOWN,BSP_BTN_CLICK);refresh(NULL);
    assert(cancels==cancelled_while_thinking+1 && fixture.phase==ONLINE_THINKING);
    fixture.background=ONLINE_BACKGROUND_OFF;refresh(NULL);
    assert(lv_obj_has_flag(background_image,LV_OBJ_FLAG_HIDDEN));snap("background-off-whale");
    fixture.theme=ONLINE_THEME_MINT;refresh(NULL);snap("background-off-mint");
    assert(!lv_obj_has_flag(face_layer,LV_OBJ_FLAG_HIDDEN));
    fixture.theme=ONLINE_THEME_WHALE;fixture.background=ONLINE_BACKGROUND_NIGHT;refresh(NULL);
    assert(!lv_obj_has_flag(background_image,LV_OBJ_FLAG_HIDDEN));snap("background-night");
    fixture.custom_background=true;fixture.background=ONLINE_BACKGROUND_CUSTOM;
    const uint8_t bg_header[]={'W','T','B','G',1,96,128,16};memcpy(custom_data,bg_header,sizeof(bg_header));
    custom_data[8]=0xff;custom_data[9]=0xcf;custom_data[10]=0x1f;custom_data[11]=0xf7;
    for(unsigned i=0;i<ONLINE_BG_PIXELS/2;i++)custom_data[40+i]=i%48<24?0x00:0x11;
    refresh(NULL);snap("background-custom");
    custom_data[8]=0;custom_data[9]=0;custom_data[10]=0x82;custom_data[11]=0x08;
    refresh(NULL);snap("background-custom-dark");
    fixture.phase=ONLINE_ERROR;strcpy(fixture.message,"请在 DSH 刷新预设");refresh(NULL);snap("custom-dark-preset-error");
    fixture.phase=ONLINE_THINKING;custom_data[8]=0xff;custom_data[9]=0xcf;custom_data[10]=0x1f;custom_data[11]=0xf7;
    /* Repeated clipped decoding must stay scanline-sized and never cache a
     * full RGB565 image or require LVGL's image-transform temporary buffers. */
    for(unsigned repeat=0;repeat<30;repeat++) {
        lv_image_decoder_dsc_t decoder;
        assert(lv_image_decoder_open(&decoder,online_background_image(custom_data),NULL)==LV_RESULT_OK);
        assert(decoder.decoded==NULL && decoder.args.no_cache);
        lv_area_t full={3,7,237,11},area={LV_COORD_MIN,LV_COORD_MIN,LV_COORD_MIN,LV_COORD_MIN};
        unsigned rows=0;
        while(lv_image_decoder_get_area(&decoder,&full,&area)==LV_RESULT_OK) {
            assert(decoder.decoded && decoder.decoded->data_size<=512);
            const uint16_t *pixels=(const uint16_t *)decoder.decoded->data;
            for(unsigned n=0;n<235;n++)assert(pixels[n]==(n+3<120?0xcfff:0xf71f));
            rows++;
        }
        assert(rows==5);lv_image_decoder_close(&decoder);
    }
    fixture.phase=ONLINE_CONNECTING;strcpy(fixture.message,"正在连接后端");refresh(NULL);
    snap("custom-background-connecting");
    online_ui_key(BSP_BTN_OK,BSP_BTN_PRESS);refresh(NULL);
    online_ui_key(BSP_BTN_OK,BSP_BTN_LONG);refresh(NULL);
    assert(menu.page==ONLINE_SETTINGS);
    online_ui_key(BSP_BTN_OK,BSP_BTN_CLICK);refresh(NULL);
    online_ui_key(BSP_BTN_DOWN,BSP_BTN_CLICK);refresh(NULL);assert(menu.selected==ONLINE_DISPLAY_BACKGROUND);
    snap("custom-settings-offline");
    online_ui_key(BSP_BTN_OK,BSP_BTN_LONG);refresh(NULL);assert(menu.page==ONLINE_HOME);
    fixture.phase=ONLINE_THINKING;
    fixture.custom_background=false;refresh(NULL);snap("background-custom-fallback");
    fixture.background=ONLINE_BACKGROUND_DAY;clock_us=2200000;refresh(NULL);
    fixture.theme=ONLINE_THEME_WHALE;fixture.phase=ONLINE_READY;fixture.mic=false;
    clock_us=4500000;refresh(NULL);snap("whale-blink");clock_us=2200000;
    fixture.phase=ONLINE_READY;fixture.mic=false;
    const unsigned volume_values[]={0,25,50,75,90,100,0};
    uint32_t previous_volume=0,muted_volume=0;
    for(unsigned i=0;i<sizeof(volume_values)/sizeof(volume_values[0]);i++) {
        fixture.volume=volume_values[i];refresh(NULL);check();
        assert(volume_icon.value==(int)fixture.volume);
        assert(!lv_obj_has_flag(home_controls,LV_OBJ_FLAG_HIDDEN));
        uint32_t rendered=icon_render_hash(&volume_icon);
        if(i)assert(rendered!=previous_volume);else muted_volume=rendered;
        if(i==6)assert(rendered==muted_volume);
        previous_volume=rendered;
    }
    snap("muted");fixture.volume=75;refresh(NULL);snap("volume");
    /* Transport status is independent of the phase and microphone switch. */
    fixture.linked=true;fixture.mic=false;refresh(NULL);
    assert(link_icon.value && !action_mic.value);
    uint32_t connected=icon_render_hash(&link_icon),mic_off=icon_render_hash(&action_mic);
    fixture.linked=false;fixture.mic=true;refresh(NULL);
    assert(!link_icon.value && action_mic.value);
    assert(icon_render_hash(&link_icon)!=connected && icon_render_hash(&action_mic)!=mic_off);
    snap("link-lost-mic-on");fixture.mic=false;fixture.linked=true;refresh(NULL);
    const int battery_values[]={-1,0,10,100,150};
    const char *battery_names[]={"battery-unknown","battery-empty","battery-low","battery-full","battery-clamped"};
    uint32_t previous_battery=0;
    for(unsigned i=0;i<sizeof(battery_values)/sizeof(battery_values[0]);i++) {
        battery_value=battery_values[i];update_battery(NULL);check();snap(battery_names[i]);
        assert(battery_icon.value==(battery_value>100?100:battery_value));
        uint32_t rendered=icon_render_hash(&battery_icon);
        if(i)assert((rendered==previous_battery)==(i==4));
        previous_battery=rendered;
    }
    battery_value=87;update_battery(NULL);
    fixture.audio_ok=false;strcpy(fixture.message,"音频不可用，请检查设备");refresh(NULL);
    assert(!strcmp(lv_label_get_text(title),fixture.message));snap("audio-unavailable");
    fixture.audio_ok=true;fixture.message[0]=0;refresh(NULL);assert(!*lv_label_get_text(title));
    fixture.phase=ONLINE_ERROR;
    strcpy(fixture.message,"后端断开，正在重连\n请检查网络和连接器");
    refresh(NULL);snap("long-message");
    assert(!strchr(lv_label_get_text(title),'\n') && strstr(lv_label_get_text(title),"请检查网络"));
    assert(lv_anim_get(title,NULL));
    uint32_t scroll_start=object_render_hash(title);
    lv_tick_inc(500);lv_anim_refr_now();refresh(NULL);
    assert(object_render_hash(title)!=scroll_start);snap("long-message-scrolled");
    int32_t previous_offset=((lv_label_t *)title)->offset.x;
    bool moved_left=false,moved_right=false;
    for(unsigned i=0;i<200 && !moved_right;i++) {
        lv_tick_inc(100);lv_anim_refr_now();
        int32_t offset=((lv_label_t *)title)->offset.x;
        moved_left|=offset<previous_offset;moved_right|=offset>previous_offset;
        previous_offset=offset;
    }
    assert(moved_left && moved_right);
    fixture.overlay=false;refresh(NULL);check();
    assert(lv_obj_has_flag(title,LV_OBJ_FLAG_HIDDEN) && !lv_anim_get(title,NULL));
    fixture.overlay=true;refresh(NULL);check();
    assert(!lv_obj_has_flag(title,LV_OBJ_FLAG_HIDDEN) && lv_anim_get(title,NULL));
    lv_tick_inc(500);lv_anim_refr_now();assert(((lv_label_t *)title)->offset.x<0);
    strcpy(fixture.message,"配对码不一致\n请核对电脑与设备");refresh(NULL);check();
    assert(((lv_label_t *)title)->offset.x==0);
    assert(!strchr(lv_label_get_text(title),'\n'));
    fixture.phase=ONLINE_READY;fixture.message[0]=0;refresh(NULL);
    check();assert(!lv_anim_get(title,NULL));
    online_ui_key(BSP_BTN_OK,BSP_BTN_PRESS);assert(!toggles);
    online_ui_key(BSP_BTN_OK,BSP_BTN_CLICK);refresh(NULL);assert(toggles==1);
    online_ui_key(BSP_BTN_UP,BSP_BTN_CLICK);refresh(NULL);assert(volumes==1);
    online_ui_key(BSP_BTN_UP,BSP_BTN_PRESS);
    online_ui_key(BSP_BTN_UP,BSP_BTN_LONG);refresh(NULL);assert(!fixture.overlay);
    online_ui_key(BSP_BTN_UP,BSP_BTN_RELEASE);
    online_ui_key(BSP_BTN_UP,BSP_BTN_CLICK);refresh(NULL);assert(volumes==1 && !fixture.overlay);
    online_ui_key(BSP_BTN_UP,BSP_BTN_PRESS);
    online_ui_key(BSP_BTN_UP,BSP_BTN_LONG);refresh(NULL);assert(fixture.overlay);
    online_ui_key(BSP_BTN_UP,BSP_BTN_RELEASE);online_ui_key(BSP_BTN_UP,BSP_BTN_CLICK);
    unsigned before_cancel=cancels;
    online_ui_key(BSP_BTN_DOWN,BSP_BTN_CLICK);refresh(NULL);assert(cancels==before_cancel+1);
    before_cancel=cancels;
    online_ui_key(BSP_BTN_DOWN,BSP_BTN_PRESS);refresh(NULL);
    online_ui_key(BSP_BTN_DOWN,BSP_BTN_RELEASE);online_ui_key(BSP_BTN_DOWN,BSP_BTN_CLICK);refresh(NULL);
    assert(cancels==before_cancel+1 && ptt_presses==1 && ptt_starts==0 && ptt_releases==1);
    before_cancel=cancels;
    online_ui_key(BSP_BTN_DOWN,BSP_BTN_PRESS);refresh(NULL);
    fixture.feedback_visible=true;strcpy(fixture.message,"正在录音，松开发送");
    online_ui_key(BSP_BTN_DOWN,BSP_BTN_LONG);refresh(NULL);assert(fixture.ptt_recording);
    assert(!strcmp(lv_label_get_text(title),fixture.message) && !lv_obj_has_flag(home_controls,LV_OBJ_FLAG_HIDDEN));snap("push-to-talk");
    online_ui_key(BSP_BTN_DOWN,BSP_BTN_RELEASE);online_ui_key(BSP_BTN_DOWN,BSP_BTN_CLICK);refresh(NULL);
    assert(cancels==before_cancel+1 && ptt_presses==2 && ptt_starts==1 && ptt_releases==2 && !fixture.ptt_recording);
    fixture.feedback_visible=false;fixture.message[0]=0;
    online_ui_key(BSP_BTN_OK,BSP_BTN_LONG);refresh(NULL);assert(setups==0);
    snap("settings-root");
    assert(lv_obj_has_flag(configuration,LV_OBJ_FLAG_HIDDEN));
    online_ui_key(BSP_BTN_DOWN,BSP_BTN_PRESS);online_ui_key(BSP_BTN_DOWN,BSP_BTN_RELEASE);
    online_ui_key(BSP_BTN_DOWN,BSP_BTN_CLICK);refresh(NULL);
    online_ui_key(BSP_BTN_OK,BSP_BTN_CLICK);refresh(NULL);assert(menu.page==ONLINE_STATUS);
    assert(!lv_obj_has_flag(configuration,LV_OBJ_FLAG_HIDDEN));snap("status-empty");
    fixture.configured=true;fixture.has_ip=true;fixture.token_set=true;
    strcpy(fixture.ssid,"Home Wi-Fi");strcpy(fixture.host,"192.168.1.10:3101");
    refresh(NULL);snap("status-configured");
    assert(!strcmp(lv_label_get_text(status_value[1]),fixture.ssid));
    assert(!strcmp(lv_label_get_text(status_value[2]),fixture.host));
    strcpy(fixture.ssid,"12345678901234567890123456789012");refresh(NULL);snap("status-long-ssid");
    online_ui_key(BSP_BTN_DOWN,BSP_BTN_CLICK);refresh(NULL);
    assert(menu.page==ONLINE_STATUS);
    online_ui_key(BSP_BTN_OK,BSP_BTN_CLICK);refresh(NULL);assert(menu.page==ONLINE_SETTINGS && menu.selected==ONLINE_MENU_STATUS);
    online_ui_key(BSP_BTN_UP,BSP_BTN_CLICK);refresh(NULL);
    online_ui_key(BSP_BTN_OK,BSP_BTN_CLICK);refresh(NULL);assert(menu.page==ONLINE_DISPLAY);
    online_theme_t before=fixture.theme;
    online_ui_key(BSP_BTN_OK,BSP_BTN_CLICK);refresh(NULL);assert(themes==1 && fixture.theme!=before);
    online_ui_key(BSP_BTN_OK,BSP_BTN_CLICK);refresh(NULL);assert(themes==2 && fixture.theme==before);
    snap("settings-empty");
    fixture.configured=true;fixture.token_set=true;strcpy(fixture.ssid,"Home Wi-Fi");strcpy(fixture.host,"192.168.1.10:3101");
    refresh(NULL);snap("settings");
    strcpy(fixture.ssid,"12345678901234567890123456789012");refresh(NULL);check();
    online_ui_key(BSP_BTN_DOWN,BSP_BTN_CLICK);refresh(NULL);
    online_ui_key(BSP_BTN_OK,BSP_BTN_CLICK);refresh(NULL);assert(fixture.background==ONLINE_BACKGROUND_NIGHT);
    online_ui_key(BSP_BTN_OK,BSP_BTN_CLICK);refresh(NULL);assert(fixture.background==ONLINE_BACKGROUND_OFF);
    assert(strstr(lv_label_get_text(lv_obj_get_child(settings_choices[ONLINE_DISPLAY_BACKGROUND],0)),"关闭"));
    snap("settings-background-off");
    online_ui_key(BSP_BTN_OK,BSP_BTN_CLICK);refresh(NULL);assert(fixture.background==ONLINE_BACKGROUND_DAY);
    online_ui_key(BSP_BTN_DOWN,BSP_BTN_CLICK);refresh(NULL);
    for(unsigned i=0;i<4;i++) {
        online_ui_key(BSP_BTN_OK,BSP_BTN_CLICK);refresh(NULL);check();
        assert(backlight==(i+1)*25 && fixture.brightness==backlight);
        char name[48];snprintf(name,sizeof(name),"brightness-%u",backlight);snap(name);
    }
    assert(brightness_changes==4);
    online_ui_key(BSP_BTN_DOWN,BSP_BTN_CLICK);refresh(NULL);
    for(unsigned i=0;i<4;i++) {
        online_ui_key(BSP_BTN_OK,BSP_BTN_CLICK);refresh(NULL);check();
        assert(fixture.idle==(online_idle_t)((i+1)%4));
        const char *states[]={"idle-3-min","idle-10-min","idle-never","idle-1-min"};
        snap(states[i]);
    }
    assert(idle_changes==4 && menu.page==ONLINE_DISPLAY);
    online_ui_key(BSP_BTN_DOWN,BSP_BTN_CLICK);refresh(NULL);
    online_ui_key(BSP_BTN_OK,BSP_BTN_CLICK);refresh(NULL);assert(!fixture.overlay);
    online_ui_key(BSP_BTN_DOWN,BSP_BTN_CLICK);refresh(NULL);
    online_ui_key(BSP_BTN_OK,BSP_BTN_CLICK);refresh(NULL);assert(menu.page==ONLINE_SETTINGS);
    online_ui_key(BSP_BTN_OK,BSP_BTN_LONG);refresh(NULL);
    assert(lv_obj_has_flag(header,LV_OBJ_FLAG_HIDDEN) && lv_obj_has_flag(home_controls,LV_OBJ_FLAG_HIDDEN));
    assert(lv_obj_has_flag(title,LV_OBJ_FLAG_HIDDEN) && !lv_anim_get(title,NULL));snap("home-clean");
    online_ui_key(BSP_BTN_UP,BSP_BTN_PRESS);online_ui_key(BSP_BTN_UP,BSP_BTN_LONG);refresh(NULL);
    assert(fixture.overlay && menu.page==ONLINE_HOME);
    online_ui_key(BSP_BTN_UP,BSP_BTN_PRESS);online_ui_key(BSP_BTN_UP,BSP_BTN_LONG);refresh(NULL);
    assert(!fixture.overlay && menu.page==ONLINE_HOME);
    online_ui_key(BSP_BTN_UP,BSP_BTN_RELEASE);online_ui_key(BSP_BTN_UP,BSP_BTN_CLICK);
    online_ui_key(BSP_BTN_OK,BSP_BTN_LONG);refresh(NULL);
    assert(menu.page==ONLINE_SETTINGS && menu.selected==ONLINE_MENU_DISPLAY);
    fixture.theme=ONLINE_THEME_WHALE;refresh(NULL);
    lv_tick_inc(200);lv_anim_refr_now();snap("whale-settings-grid");
    online_ui_key(BSP_BTN_DOWN,BSP_BTN_PRESS);online_ui_key(BSP_BTN_DOWN,BSP_BTN_LONG);refresh(NULL);
    assert(menu.selected==ONLINE_MENU_SOUND && !fixture.overlay);
    online_ui_key(BSP_BTN_DOWN,BSP_BTN_RELEASE);online_ui_key(BSP_BTN_DOWN,BSP_BTN_CLICK);refresh(NULL);
    assert(menu.selected==ONLINE_MENU_SOUND);
    lv_obj_t *sound_card=settings_choices[ONLINE_MENU_SOUND];
    assert(lv_anim_get(sound_card,draw_bounce));
    uint32_t lifted=object_render_hash(sound_card);
    for(unsigned frame=0;frame<7;frame++) {
        char name[32];snprintf(name,sizeof(name),"settings-bounce-%u",frame);
        snap(name);lv_tick_inc(33);lv_anim_refr_now();
    }
    uint32_t settled=object_render_hash(sound_card);
    assert(lifted!=settled && !lv_anim_get(sound_card,draw_bounce));
    refresh(NULL);assert(object_render_hash(sound_card)==settled);
    fixture.theme=ONLINE_THEME_MINT;refresh(NULL);snap("mint-settings-grid");
    online_ui_key(BSP_BTN_UP,BSP_BTN_PRESS);online_ui_key(BSP_BTN_UP,BSP_BTN_LONG);refresh(NULL);
    online_ui_key(BSP_BTN_UP,BSP_BTN_RELEASE);online_ui_key(BSP_BTN_UP,BSP_BTN_CLICK);refresh(NULL);
    assert(menu.selected==ONLINE_MENU_DISPLAY && !fixture.overlay);
    online_ui_key(BSP_BTN_DOWN,BSP_BTN_CLICK);refresh(NULL);
    assert(!lv_anim_get(settings_choices[ONLINE_MENU_DISPLAY],draw_bounce));
    online_ui_key(BSP_BTN_DOWN,BSP_BTN_CLICK);refresh(NULL);
    online_ui_key(BSP_BTN_OK,BSP_BTN_CLICK);refresh(NULL);assert(menu.page==ONLINE_SOUND);
    assert(!lv_anim_get(sound_card,draw_bounce));snap("sounds");
    fixture.volume=100;refresh(NULL);
    online_ui_key(BSP_BTN_OK,BSP_BTN_CLICK);refresh(NULL);assert(fixture.volume==0);
    snap("sound-muted");
    online_ui_key(BSP_BTN_DOWN,BSP_BTN_CLICK);refresh(NULL);
    online_ui_key(BSP_BTN_OK,BSP_BTN_CLICK);refresh(NULL);assert(!fixture.cues);snap("cues-off");
    online_ui_key(BSP_BTN_OK,BSP_BTN_CLICK);refresh(NULL);assert(fixture.cues);
    online_ui_key(BSP_BTN_DOWN,BSP_BTN_CLICK);refresh(NULL);
    online_ui_key(BSP_BTN_OK,BSP_BTN_CLICK);refresh(NULL);assert(menu.page==ONLINE_SETTINGS);
    online_ui_key(BSP_BTN_OK,BSP_BTN_LONG);refresh(NULL);
    fixture.theme=ONLINE_THEME_MINT;fixture.phase=ONLINE_LISTENING;fixture.mic=true;refresh(NULL);
    assert(!lv_obj_has_flag(ears[0],LV_OBJ_FLAG_HIDDEN));
    fixture.phase=ONLINE_SPEAKING;fixture.level=1000;refresh(NULL);check();int y=lv_obj_get_height(mouth);
    clock_us+=5000000;fixture.level=19000;refresh(NULL);check();
    assert(lv_obj_get_height(mouth)>y);
    assert(lv_obj_has_flag(ears[0],LV_OBJ_FLAG_HIDDEN));
    /* Standby preserves the desired mic, while runtime pauses actual input. */
    menu.page=ONLINE_HOME;fixture.phase=ONLINE_LISTENING;fixture.mic=true;
    refresh(NULL);clock_us+=60000000;refresh(NULL);
    assert(backlight==0 && fixture.mic && standby && standby_entries);
    unsigned previous_toggles=toggles,previous_cancels=cancels,previous_volumes=volumes,previous_ptt=ptt_presses;
    for(unsigned i=0;i<3;i++) {
        bsp_btn_t key=i==0?BSP_BTN_OK:i==1?BSP_BTN_UP:BSP_BTN_DOWN;
        unsigned before_wake=wake_requests;
        // The key requests wake before any UI refresh: a stopped LVGL tick
        // must not strand the device asleep.
        online_ui_key(key,BSP_BTN_PRESS);assert(!standby && wake_requests>before_wake);
        refresh(NULL);assert(backlight==100);
        if(key==BSP_BTN_DOWN) {online_ui_key(key,BSP_BTN_LONG);online_ui_key(key,BSP_BTN_RELEASE);}
        online_ui_key(key,BSP_BTN_CLICK);refresh(NULL);
        assert(toggles==previous_toggles && cancels==previous_cancels && volumes==previous_volumes && ptt_presses==previous_ptt);
        clock_us+=60000000;refresh(NULL);assert(backlight==0);
    }
    online_ui_key(BSP_BTN_OK,BSP_BTN_PRESS);refresh(NULL);
    online_ui_key(BSP_BTN_OK,BSP_BTN_LONG);refresh(NULL);assert(menu.page==ONLINE_HOME);
    online_ui_key(BSP_BTN_OK,BSP_BTN_RELEASE);
    clock_us+=60000000;refresh(NULL);assert(backlight==0);
    bool overlay_before_wake=fixture.overlay;
    online_ui_key(BSP_BTN_UP,BSP_BTN_PRESS);refresh(NULL);assert(backlight==100);
    online_ui_key(BSP_BTN_UP,BSP_BTN_LONG);online_ui_key(BSP_BTN_UP,BSP_BTN_RELEASE);
    online_ui_key(BSP_BTN_UP,BSP_BTN_CLICK);refresh(NULL);
    assert(fixture.overlay==overlay_before_wake && volumes==previous_volumes);
    online_ui_key(BSP_BTN_OK,BSP_BTN_PRESS);
    online_ui_key(BSP_BTN_OK,BSP_BTN_CLICK);refresh(NULL);assert(toggles==previous_toggles+1);
    clock_us+=60000000;refresh(NULL);assert(backlight==0);
    fixture.phase=ONLINE_THINKING;refresh(NULL);assert(backlight==100);
    clock_us+=120000000;refresh(NULL);assert(backlight==100);
    fixture.phase=ONLINE_SPEAKING;clock_us+=120000000;refresh(NULL);assert(backlight==100);
    fixture.phase=ONLINE_READY;refresh(NULL);clock_us+=60000000;refresh(NULL);assert(backlight==0);
    fixture.keep_awake=true;online_ui_activity();refresh(NULL);
    clock_us+=120000000;refresh(NULL);assert(backlight==100 && !standby);
    fixture.keep_awake=false;refresh(NULL);clock_us+=60000000;refresh(NULL);assert(backlight==0 && standby);
    fixture.phase=ONLINE_SETUP;refresh(NULL);assert(backlight==100);
    clock_us+=120000000;refresh(NULL);assert(backlight==100);
    fixture.phase=ONLINE_READY;refresh(NULL);clock_us+=60000000;refresh(NULL);assert(backlight==0);
    unsigned transitions=backlight_changes;refresh(NULL);assert(backlight_changes==transitions);
    /* Apply each setting to the real UI timer, including disabling an asleep screen. */
    for(unsigned i=0;i<=ONLINE_IDLE_NEVER;i++) {
        fixture.idle=(online_idle_t)i;
        online_ui_key(BSP_BTN_UP,BSP_BTN_PRESS);refresh(NULL);
        online_ui_key(BSP_BTN_UP,BSP_BTN_RELEASE);
        assert(backlight==100);
        uint64_t timeout=online_idle_timeout_ms(fixture.idle);
        if(timeout) {
            clock_us+=(timeout-1)*1000;refresh(NULL);assert(backlight==100);
            clock_us+=1000;refresh(NULL);assert(backlight==0);
        } else {
            clock_us+=INT64_C(86400000000);refresh(NULL);assert(backlight==100);
        }
    }
    /* Wake restores the chosen level; changing a preference while asleep
     * never lights the panel until a real wake event occurs. */
    fixture.phase=ONLINE_LISTENING;fixture.idle=ONLINE_IDLE_1_MIN;
    for(unsigned percent=25;percent<=100;percent+=25) {
        fixture.brightness=percent;atomic_store(&pending_activity,true);refresh(NULL);
        assert(backlight==percent);
        clock_us+=60000000;refresh(NULL);assert(backlight==0);
        fixture.brightness=online_brightness_next(percent);refresh(NULL);assert(backlight==0);
        unsigned before=toggles;
        online_ui_key(BSP_BTN_OK,BSP_BTN_PRESS);refresh(NULL);assert(backlight==fixture.brightness);
        online_ui_key(BSP_BTN_OK,BSP_BTN_CLICK);refresh(NULL);assert(toggles==before);
        online_ui_key(BSP_BTN_OK,BSP_BTN_RELEASE);
    }
    fixture.brightness=100;fixture.overlay=true;fixture.idle=ONLINE_IDLE_NEVER;fixture.phase=ONLINE_SETUP;
    menu.page=ONLINE_HOME;strcpy(fixture.message,"DSH-Voice-ABCDEF");refresh(NULL);
    assert(setup_qr[0] && setup_qr[1]);snap("setup-qr");
    uint32_t code_hash=object_render_hash(setup_qr[0]);refresh(NULL);
    assert(code_hash==object_render_hash(setup_qr[0]));
    fixture.phase=ONLINE_READY;refresh(NULL);assert(!setup_qr[0] && !setup_qr[1]);
    fixture.phase=ONLINE_SETUP;refresh(NULL);assert(setup_qr[0] && setup_qr[1]);
    assert(code_hash==object_render_hash(setup_qr[0])); /* Re-entering the same hotspot restores freed QR buffers. */
    menu.page=ONLINE_SETTINGS;menu.selected=ONLINE_MENU_CONNECTION;refresh(NULL);
    online_ui_key(BSP_BTN_OK,BSP_BTN_PRESS);online_ui_key(BSP_BTN_OK,BSP_BTN_RELEASE);
    online_ui_key(BSP_BTN_OK,BSP_BTN_CLICK);refresh(NULL);assert(menu.page==ONLINE_CONNECTION);
    snap("networks-empty");
    online_ui_key(BSP_BTN_OK,BSP_BTN_CLICK);refresh(NULL);
    assert(menu.page==ONLINE_HOME && !setups); /* Already in provisioning, return to QR. */
    unsigned before_resume_mic=toggles;
    online_ui_key(BSP_BTN_OK,BSP_BTN_CLICK);refresh(NULL);
    assert(!network_switches && toggles==before_resume_mic); /* First boot has no saved network. */
    fixture.phase=ONLINE_READY;
    online_ui_key(BSP_BTN_OK,BSP_BTN_LONG);refresh(NULL);
    online_ui_key(BSP_BTN_UP,BSP_BTN_CLICK);refresh(NULL);
    online_ui_key(BSP_BTN_OK,BSP_BTN_CLICK);refresh(NULL);assert(menu.page==ONLINE_CONNECTION);
    online_ui_key(BSP_BTN_UP,BSP_BTN_CLICK);refresh(NULL);
    online_ui_key(BSP_BTN_OK,BSP_BTN_CLICK);refresh(NULL);assert(menu.page==ONLINE_SETTINGS);
    fixture.profile_count=3;strcpy(fixture.profile_ssids[0],"Home");strcpy(fixture.profile_ssids[1],"Office");
    strcpy(fixture.profile_ssids[2],"12345678901234567890123456789012");
    online_ui_key(BSP_BTN_OK,BSP_BTN_CLICK);refresh(NULL);snap("networks");
    online_ui_key(BSP_BTN_OK,BSP_BTN_CLICK);refresh(NULL);assert(!network_switches);
    online_ui_key(BSP_BTN_DOWN,BSP_BTN_CLICK);refresh(NULL);
    online_ui_key(BSP_BTN_OK,BSP_BTN_CLICK);refresh(NULL);assert(network_switches==1 && selected_network==1);
    fixture.profile_save_failed=true;refresh(NULL);snap("networks-save-failed");fixture.profile_save_failed=false;
    online_ui_key(BSP_BTN_DOWN,BSP_BTN_CLICK);refresh(NULL);snap("networks-long-ssid");
    online_ui_key(BSP_BTN_OK,BSP_BTN_CLICK);refresh(NULL);assert(network_switches==2 && selected_network==2);
    online_ui_key(BSP_BTN_DOWN,BSP_BTN_CLICK);refresh(NULL);
    snap("networks-reconfigure");
    online_ui_key(BSP_BTN_DOWN,BSP_BTN_CLICK);refresh(NULL);
    online_ui_key(BSP_BTN_OK,BSP_BTN_CLICK);refresh(NULL);assert(menu.page==ONLINE_SETTINGS);
    online_ui_key(BSP_BTN_OK,BSP_BTN_CLICK);refresh(NULL);
    online_ui_key(BSP_BTN_UP,BSP_BTN_CLICK);refresh(NULL);
    online_ui_key(BSP_BTN_UP,BSP_BTN_CLICK);refresh(NULL);
    online_ui_key(BSP_BTN_OK,BSP_BTN_CLICK);refresh(NULL);
    assert(menu.page==ONLINE_HOME && setups==1);
    fixture.phase=ONLINE_SETUP;
    strcpy(fixture.message,"DSH-Voice-ABCDEF");
    refresh(NULL);snap("setup-resume");
    online_ui_key(BSP_BTN_OK,BSP_BTN_CLICK);refresh(NULL);
    assert(network_switches==3 && selected_network==0 && toggles==before_resume_mic);
    online_ui_key(BSP_BTN_OK,BSP_BTN_LONG);refresh(NULL);
    online_ui_key(BSP_BTN_UP,BSP_BTN_CLICK);refresh(NULL);
    online_ui_key(BSP_BTN_OK,BSP_BTN_CLICK);refresh(NULL);
    snap("networks-resume");
    online_ui_key(BSP_BTN_OK,BSP_BTN_CLICK);refresh(NULL);
    assert(network_switches==4 && selected_network==0);
    online_ui_key(BSP_BTN_UP,BSP_BTN_CLICK);refresh(NULL);
    online_ui_key(BSP_BTN_OK,BSP_BTN_CLICK);refresh(NULL);assert(menu.page==ONLINE_SETTINGS);
    online_ui_key(BSP_BTN_UP,BSP_BTN_CLICK);refresh(NULL);assert(menu.selected==ONLINE_MENU_HOME);
    online_ui_key(BSP_BTN_OK,BSP_BTN_CLICK);refresh(NULL);
    assert(menu.page==ONLINE_HOME && network_switches==5 && selected_network==0);
    fixture.phase=ONLINE_READY;buttons_available=false;refresh(NULL);check();
    assert(backlight==100 && strstr(lv_label_get_text(title),"按键不可用"));
    puts("Online UI: both themes, phases, glyphs, clipping, bounds and controls PASS");
}
