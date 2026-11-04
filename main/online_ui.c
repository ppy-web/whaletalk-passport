#include "online_ui.h"
#include "online_runtime.h"
#include "bsp_battery.h"
#include "bsp_display.h"
#include "esp_timer.h"
#include "lvgl.h"
#include "whale_avatar.h"
#include <stdio.h>
#include <stdatomic.h>
#include <string.h>
LV_FONT_DECLARE(mouthy_bean_zh_12);
LV_FONT_DECLARE(mouthy_bean_zh_16);
#define YELLOW 0xA7E8DC
#define INK 0x173E3A
#define PAPER 0xE9F8F3
static lv_obj_t *face_layer,*pupils[2],*lids[2],*closed_eyes[2],*ears[2],*mouth,*teeth,*tongue,*smile;
static lv_obj_t *title,*caption,*footer,*volume_label,*battery,*setup_panel,*setup_text;
static lv_obj_t *app_screen,*whale_layer,*whale_image;
static lv_obj_t *settings_layer,*confirm_layer,*settings_help,*wifi_value,*password_value,*host_value,*token_value;
static lv_obj_t *settings_choices[4],*confirm_choices[2];
static online_menu_t menu;
static atomic_uint pending_key;
static atomic_bool pending_activity,screen_asleep;
static online_screen_t screen_idle;
static bool buttons_available;
static lv_obj_t *shape(lv_obj_t *p,int x,int y,int w,int h,uint32_t color,int radius) {
    lv_obj_t *o=lv_obj_create(p); lv_obj_remove_style_all(o);
    lv_obj_remove_flag(o,LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(o,x,y); lv_obj_set_size(o,w,h);
    lv_obj_set_style_bg_color(o,lv_color_hex(color),0); lv_obj_set_style_bg_opa(o,LV_OPA_COVER,0);
    lv_obj_set_style_radius(o,radius,0); return o;
}
static lv_obj_t *label(lv_obj_t *p,const char *value,int x,int y,int w,int h,bool small,uint32_t color) {
    lv_obj_t *o=lv_label_create(p); lv_obj_set_pos(o,x,y); lv_obj_set_size(o,w,h);
    lv_obj_set_style_text_font(o,small?&mouthy_bean_zh_12:&mouthy_bean_zh_16,0);
    lv_obj_set_style_text_color(o,lv_color_hex(color),0);
    lv_obj_set_style_text_align(o,LV_TEXT_ALIGN_CENTER,0);
    lv_obj_set_style_text_line_space(o,4,0);
    lv_label_set_long_mode(o,LV_LABEL_LONG_WRAP); lv_label_set_text(o,value); return o;
}
static void visibility(lv_obj_t *o,bool show) {
    if(show==!lv_obj_has_flag(o,LV_OBJ_FLAG_HIDDEN))return;
    if(show)lv_obj_remove_flag(o,LV_OBJ_FLAG_HIDDEN);else lv_obj_add_flag(o,LV_OBJ_FLAG_HIDDEN);
}
static void text_changed(lv_obj_t *o,const char *value) {
    if(strcmp(lv_label_get_text(o),value))lv_label_set_text(o,value);
}
static void eye(int i,int x) {
    shape(face_layer,x,89,96,101,0x63A99D,48);
    lv_obj_t *rim=shape(face_layer,x,82,96,100,0x766D64,47);
    lv_obj_set_style_border_width(rim,2,0); lv_obj_set_style_border_color(rim,lv_color_hex(0x625448),0);
    shape(rim,4,3,88,92,0xE7E4CF,43);
    shape(rim,8,7,80,84,0x9D9688,39);
    lv_obj_t *white=shape(rim,10,10,76,78,0xFFFEF5,38);
    /* Keep children inside the rounded eye geometrically. Corner clipping
     * creates an off-screen alpha layer and can exhaust the TLS-era heap. */
    pupils[i]=shape(white,22,25,37,43,0x9C6842,20);
    shape(pupils[i],5,5,27,33,0x593C2D,15);
    shape(pupils[i],10,8,19,26,0x231D1B,12);
    shape(pupils[i],6,6,11,13,0xFFFFFF,7);
    shape(pupils[i],25,27,5,6,0xEED4AC,3);
    lids[i]=shape(white,0,0,76,0,YELLOW,38);
    closed_eyes[i]=shape(white,10,53,56,4,INK,2);
    shape(rim,20,5,27,2,0xFFFFFF,1);
}
static void refresh(lv_timer_t *timer) {
    (void)timer;
    uint64_t now=esp_timer_get_time()/1000;
    bool activity=atomic_exchange(&pending_activity,false);
    unsigned key=atomic_exchange(&pending_key,0);
    /* A command queued before the idle deadline still counts as activity. */
    activity=activity || key!=0;
    if(key) {
        switch(online_menu_key(&menu,(online_key_t)key)) {
            case ONLINE_ACTION_MIC: online_toggle_mic();break;
            case ONLINE_ACTION_VOLUME: online_volume();break;
            case ONLINE_ACTION_CANCEL: online_cancel();break;
            case ONLINE_ACTION_SETUP: online_setup();break;
            case ONLINE_ACTION_THEME: online_cycle_theme();break;
            case ONLINE_ACTION_IDLE: online_cycle_idle();break;
            default: break;
        }
    }
    online_status_t s;online_snapshot(&s);
    bool was_asleep=atomic_load(&screen_asleep);
    bool asleep=online_screen_step(&screen_idle,now,activity,
        !buttons_available || s.phase==ONLINE_SETUP || s.phase==ONLINE_THINKING || s.phase==ONLINE_SPEAKING,
        online_idle_timeout_ms(s.idle));
    if(asleep!=was_asleep) {
        atomic_store(&screen_asleep,asleep);
        bsp_display_backlight(asleep?0:100);
    }
    /* Keep the lightweight timer for wake events; stop animation/invalidation. */
    if(asleep)return;
    bool home=menu.page==ONLINE_HOME,setup=s.phase==ONLINE_SETUP;
    bool listen=s.mic && (s.phase==ONLINE_LISTENING || s.phase==ONLINE_READY);
    bool speaking=s.phase==ONLINE_SPEAKING;
    bool whale=s.theme==ONLINE_THEME_WHALE;
    uint32_t background=whale?0xE7F0FF:YELLOW;
    static int applied_theme=-1;
    if(applied_theme!=(int)s.theme) {
        lv_obj_set_style_bg_color(app_screen,lv_color_hex(background),0);
        lv_obj_set_style_bg_color(settings_layer,lv_color_hex(background),0);
        lv_obj_set_style_bg_color(confirm_layer,lv_color_hex(background),0);
        applied_theme=s.theme;
    }
    text_changed(title,home?"鲸语":menu.page==ONLINE_SETTINGS?"设置":"重新配置？");
    visibility(face_layer,home && !setup && !whale);visibility(whale_layer,home && !setup && whale);
    visibility(caption,home && !setup);visibility(setup_panel,home && setup);
    if(home && !setup && whale) {
        static int displayed_frame=-1;
        online_avatar_t frame=online_avatar_frame(s.phase,s.mic,s.level,now);
        if(displayed_frame!=(int)frame) {
            lv_image_set_src(whale_image,whale_avatar_frames[frame]);
            displayed_frame=frame;
        }
    }
    visibility(volume_label,home && !setup && buttons_available);
    static unsigned displayed_volume=101;
    if(displayed_volume!=s.volume) {
        if(s.volume)lv_label_set_text_fmt(volume_label,"音量 %u%%",s.volume);
        else lv_label_set_text(volume_label,"静音 0%");
        displayed_volume=s.volume;
    }
    visibility(footer,home);visibility(settings_layer,menu.page==ONLINE_SETTINGS);
    visibility(confirm_layer,menu.page==ONLINE_CONFIRM);
    if(menu.page==ONLINE_SETTINGS) {
        text_changed(settings_help,s.idle_save_failed?"息屏未保存，请重试":s.theme_save_failed?"界面未保存，请重试":"选择项目，按确定进入");
        lv_label_set_text_fmt(wifi_value,"Wi-Fi：%s",s.configured?s.ssid:"未配置");
        lv_label_set_text_fmt(password_value,"Wi-Fi 密码：%s",!s.configured?"未配置":s.password_set?"已保存":"开放网络");
        lv_label_set_text_fmt(host_value,"电脑：%s",s.configured?s.host:"未配置");
        lv_label_set_text_fmt(token_value,"配对码：%s",s.configured&&s.token_set?"已保存":"未配置");
        text_changed(lv_obj_get_child(settings_choices[0],0),whale?"界面：鲸鱼娘":"界面：薄荷表情");
        static const char *const idle_labels[]={"息屏：1 分钟","息屏：3 分钟","息屏：10 分钟","息屏：不息屏"};
        text_changed(lv_obj_get_child(settings_choices[1],0),idle_labels[online_idle_normalize(s.idle)]);
    }
    static int settings_selected=-1,confirm_selected=-1;
    if(menu.page==ONLINE_SETTINGS && settings_selected!=(int)menu.selected) {
        for(unsigned i=0;i<4;i++) {
            lv_obj_set_style_bg_color(settings_choices[i],lv_color_hex(menu.selected==i?INK:PAPER),0);
            lv_obj_set_style_text_color(lv_obj_get_child(settings_choices[i],0),lv_color_hex(menu.selected==i?PAPER:INK),0);
        }
        settings_selected=menu.selected;
    }
    if(menu.page==ONLINE_CONFIRM && confirm_selected!=(int)menu.selected) {
        for(unsigned i=0;i<2;i++) {
            lv_obj_set_style_bg_color(confirm_choices[i],lv_color_hex(menu.selected==i?INK:PAPER),0);
            lv_obj_set_style_text_color(lv_obj_get_child(confirm_choices[i],0),lv_color_hex(menu.selected==i?PAPER:INK),0);
        }
        confirm_selected=menu.selected;
    }
    if(setup)text_changed(setup_text,s.message);
    for(int i=0;i<2;i++) {
        visibility(ears[i],listen);lv_obj_set_y(ears[i],listen?49:82);
        bool blink=!listen && !speaking && now%4300<120;
        lv_obj_set_height(lids[i],blink?74:0);visibility(closed_eyes[i],blink);
        /* Keep eye contact while speaking, without the idle horizontal drift. */
        lv_obj_set_pos(pupils[i],22,25);
    }
    unsigned mh=speaking?22+s.level/1100:24;if(mh>46)mh=46;
    unsigned mw=speaking?64:60;
    lv_obj_set_size(mouth,mw,mh);
    lv_obj_set_pos(mouth,(240-mw)/2,218-mh/2);
    lv_obj_set_pos(teeth,(mw-42)/2,1);
    lv_obj_set_pos(tongue,(mw-30)/2,mh-12);
    visibility(mouth,speaking);
    visibility(smile,!speaking);
    visibility(tongue,speaking);
    lv_obj_set_width(footer,setup||!buttons_available?216:126);
    lv_obj_set_style_text_align(footer,setup||!buttons_available?LV_TEXT_ALIGN_CENTER:LV_TEXT_ALIGN_LEFT,0);
    lv_obj_set_y(footer,294);lv_obj_set_height(footer,22);
    text_changed(caption,(s.phase==ONLINE_ERROR || s.phase==ONLINE_CONNECTING)&&s.message[0]?s.message:
        s.phase==ONLINE_READY&&!s.mic?"按确定开麦":online_phase_text(s.phase));
    text_changed(footer,!buttons_available?"按键不可用，请检查设备":setup?
        "长按确定 · 设置":"长按确定 · 设置");
}
static void update_battery(lv_timer_t *timer) {
    (void)timer;if(atomic_load(&screen_asleep))return;
    int soc=bsp_battery_soc();
    if(soc<0)lv_label_set_text(battery,"--%");else lv_label_set_text_fmt(battery,"%d%%",soc>100?100:soc);
}
void online_ui_enter(bool buttons_ok) {
    buttons_available=buttons_ok;
    lv_obj_t *screen=shape(NULL,0,0,240,320,0xE7F0FF,0);app_screen=screen;
    title=label(screen,"鲸语",8,8,170,20,false,INK);
    battery=label(screen,"--%",191,8,44,20,true,INK);
    face_layer=shape(screen,0,0,240,320,YELLOW,0);lv_obj_move_background(face_layer);
    for(int i=0;i<2;i++) {
        ears[i]=shape(face_layer,i?178:37,82,25,68,0x3C887D,12);
        shape(ears[i],3,3,19,62,0xD3F4EB,9);shape(ears[i],7,9,11,45,0x91CABE,6);
    }
    eye(0,19);eye(1,125);
    shape(face_layer,27,191,37,12,0xE68048,6);shape(face_layer,177,191,37,12,0xE68048,6);
    mouth=shape(face_layer,90,206,60,24,0x49291F,18);
    /* Teeth/tongue stay inside the rounded mouth without an alpha layer. */
    teeth=shape(mouth,9,1,42,7,0xFFFEF5,3);
    tongue=shape(mouth,15,12,30,10,0xF28D89,7);
    smile=lv_arc_create(face_layer);lv_obj_remove_style_all(smile);
    lv_obj_remove_flag(smile,LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_pos(smile,88,162);lv_obj_set_size(smile,64,64);
    lv_arc_set_bg_angles(smile,40,140);lv_arc_set_angles(smile,40,140);
    lv_obj_set_style_arc_color(smile,lv_color_hex(0x49291F),LV_PART_INDICATOR);
    lv_obj_set_style_arc_opa(smile,LV_OPA_COVER,LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(smile,4,LV_PART_INDICATOR);
    lv_obj_set_style_arc_rounded(smile,true,LV_PART_INDICATOR);
    whale_layer=shape(screen,24,42,192,192,0xE7F0FF,0);
    whale_image=lv_image_create(whale_layer);
    lv_obj_set_pos(whale_image,0,0);
    lv_image_set_src(whale_image,whale_avatar_frames[ONLINE_AVATAR_IDLE]);
    caption=label(screen,"",12,246,216,46,false,INK);
    setup_panel=shape(screen,8,42,224,240,PAPER,14);
    label(setup_panel,"连接设备热点",8,10,208,24,false,INK);
    setup_text=label(setup_panel,"",8,46,208,24,false,INK);
    label(setup_panel,"无需密码，直接连接",8,78,208,24,false,INK);
    label(setup_panel,"浏览器打开",8,116,208,24,false,INK);
    lv_obj_t *address=label(setup_panel,"192.168.4.1",8,148,208,30,false,INK);
    lv_obj_set_style_text_font(address,&lv_font_montserrat_20,0);
    label(setup_panel,"电脑 IP + 4 位配对码",8,196,208,24,false,INK);
    footer=label(screen,"",12,294,216,22,false,INK);
    volume_label=label(screen,"",144,294,84,22,false,INK);
    lv_obj_set_style_text_align(volume_label,LV_TEXT_ALIGN_RIGHT,0);
    lv_label_set_long_mode(volume_label,LV_LABEL_LONG_CLIP);
    settings_layer=shape(screen,0,35,240,285,YELLOW,0);
    settings_help=label(settings_layer,"",8,0,224,22,false,INK);
    lv_obj_t *configuration=shape(settings_layer,8,26,224,114,PAPER,12);
    wifi_value=label(configuration,"",8,4,208,22,false,INK);
    password_value=label(configuration,"",8,26,208,22,false,INK);
    host_value=label(configuration,"",8,48,208,40,false,INK);
    token_value=label(configuration,"",8,88,208,22,false,INK);
    lv_label_set_long_mode(wifi_value,LV_LABEL_LONG_DOT);
    const char *choices[]={"界面：鲸鱼娘","息屏：1 分钟","返回首页","重新配置"};
    for(unsigned i=0;i<4;i++) {
        settings_choices[i]=shape(settings_layer,16,146+i*28,208,26,PAPER,9);
        label(settings_choices[i],choices[i],4,2,200,22,false,INK);
    }
    label(settings_layer,"上/下选择 · 确定进入",8,262,224,22,false,INK);
    confirm_layer=shape(screen,0,35,240,285,YELLOW,0);
    label(confirm_layer,"连接热点，无需密码\n浏览器打开 192.168.4.1\n取消保留当前配置",12,50,216,78,false,INK);
    const char *confirm[]={"取消，保留当前配置","确认重新配置"};
    for(unsigned i=0;i<2;i++) {
        confirm_choices[i]=shape(confirm_layer,12,164+i*38,216,32,PAPER,9);
        label(confirm_choices[i],confirm[i],4,6,208,22,false,INK);
    }
    label(confirm_layer,"上/下选择 · 确定执行\n长按确定返回首页",8,236,224,46,false,INK);
    refresh(NULL);update_battery(NULL);lv_screen_load(screen);
    /* One application-lifetime screen. Workers never retain LVGL objects. */
    lv_timer_create(refresh,50,NULL);lv_timer_create(update_battery,15000,NULL);
}
void online_ui_key(bsp_btn_t key,bsp_btn_ev_t ev) {
    /* This callback owns the gesture flag. RELEASE precedes CLICK, so retain it
     * until the next PRESS and suppress both short and long wake gestures. */
    static bool wake_gesture;
    if(ev==BSP_BTN_PRESS) {
        wake_gesture=atomic_load(&screen_asleep);
        atomic_store(&pending_activity,true);return;
    }
    unsigned command=0;
    if(ev==BSP_BTN_LONG && key==BSP_BTN_OK)command=ONLINE_KEY_LONG;
    else if(ev==BSP_BTN_CLICK)command=key==BSP_BTN_OK?ONLINE_KEY_OK:key==BSP_BTN_UP?ONLINE_KEY_UP:ONLINE_KEY_DOWN;
    /* Button callbacks never touch LVGL. Consume one action on its task. */
    if(command) {
        if(wake_gesture)return;
        atomic_store(&pending_activity,true);
        if(atomic_load(&screen_asleep))return;
        unsigned empty=0;atomic_compare_exchange_strong(&pending_key,&empty,command);
    }
}
