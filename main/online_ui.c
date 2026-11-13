#include "online_ui.h"
#include "online_runtime.h"
#include "bsp_battery.h"
#include "bsp_display.h"
#include "esp_timer.h"
#include "lvgl.h"
#include "whale_avatar.h"
#include "whale_avatar_view.h"
#include "online_ui_icons.h"
#include "online_background.h"
#include "online_background_image.h"
#include "whale_backgrounds.h"
#include <stdio.h>
#include <stdatomic.h>
#include <string.h>
LV_FONT_DECLARE(mouthy_bean_zh_12);
LV_FONT_DECLARE(mouthy_bean_zh_16);
#define YELLOW 0xA7E8DC
#define INK 0x173E3A
#define PAPER 0xE9F8F3
static lv_obj_t *face_layer,*pupils[2],*lids[2],*closed_eyes[2],*ears[2],*mouth,*teeth,*tongue,*smile;
static lv_obj_t *title,*header,*caption,*footer,*setup_panel,*setup_text;
static lv_obj_t *home_controls,*background_image,*settings_list;
static ui_icon_t link_icon,mic_icon,battery_icon,phase_icon,volume_icon;
static ui_icon_t up_icon,ok_icon,down_icon,action_mic,stop_icon;
static lv_obj_t *app_screen,*whale_layer,*whale_image;
static lv_obj_t *settings_layer,*configuration,*connection_back,*settings_footer,*confirm_layer,*settings_help,*status_text;
static lv_obj_t *settings_choices[ONLINE_DISPLAY_COUNT],*confirm_choices[2];
static online_menu_t menu;
static atomic_uint pending_key;
static atomic_bool pending_activity,screen_asleep,ptt_home;
static online_screen_t screen_idle;
static bool buttons_available;
void online_ui_activity(void) {atomic_store(&pending_activity,true);}
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
static lv_obj_t *key_hint(lv_obj_t *parent,int x,int y,int w,int h) {
    lv_obj_t *o=shape(parent,x,y,w,h,INK,6);
    lv_obj_remove_flag(o,LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_opa(o,LV_OPA_TRANSP,0);
    lv_obj_set_style_border_color(o,lv_color_hex(INK),0);
    lv_obj_set_style_border_width(o,1,0);
    return o;
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
            case ONLINE_ACTION_BACKGROUND: online_cycle_background();break;
            case ONLINE_ACTION_BRIGHTNESS: online_cycle_brightness();break;
            case ONLINE_ACTION_OVERLAY: online_toggle_overlay();break;
            case ONLINE_ACTION_CUE: online_toggle_cues();break;
            default: break;
        }
        if(menu.page==ONLINE_CONNECTION && (key==ONLINE_KEY_UP || key==ONLINE_KEY_DOWN)) {
            lv_obj_update_layout(configuration);
            lv_obj_scroll_by_bounded(configuration,0,key==ONLINE_KEY_UP?36:-36,LV_ANIM_OFF);
        }
    }
    if(menu.page!=ONLINE_HOME)online_ptt_abort();
    online_status_t s;online_snapshot(&s);
    bool was_asleep=atomic_load(&screen_asleep);
    bool asleep=online_screen_step(&screen_idle,now,activity,
        !buttons_available || s.keep_awake || s.ptt_recording || s.phase==ONLINE_SETUP || s.phase==ONLINE_THINKING || s.phase==ONLINE_SPEAKING,
        online_idle_timeout_ms(s.idle));
    if(asleep!=was_asleep) {
        if(!online_set_standby(asleep)) {
            asleep=false;online_screen_step(&screen_idle,now,true,false,0);
        }
        atomic_store(&screen_asleep,asleep);
    }
    static unsigned applied_brightness=101;
    unsigned desired_brightness=asleep?0:online_brightness_normalize(s.brightness);
    if(applied_brightness!=desired_brightness) {
        bsp_display_backlight((uint8_t)desired_brightness);applied_brightness=desired_brightness;
    }
    /* The maintenance worker pauses LVGL after this callback releases its lock. */
    if(asleep)return;
    bool home=menu.page==ONLINE_HOME,setup=s.phase==ONLINE_SETUP;
    atomic_store(&ptt_home,home && !setup);
    bool listen=(s.mic || s.ptt_recording) && (s.phase==ONLINE_LISTENING || s.phase==ONLINE_READY);
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
    bool show_overlay=!home || setup || s.overlay;
    text_changed(title,home?"鲸语":menu.page==ONLINE_SETTINGS?"设置":menu.page==ONLINE_DISPLAY?"系统设置":menu.page==ONLINE_CONNECTION?"状态信息":"重新配置？");
    visibility(header,show_overlay);visibility(title,show_overlay);
    visibility(link_icon.obj,show_overlay);visibility(mic_icon.obj,show_overlay);visibility(battery_icon.obj,show_overlay);
    static int displayed_background=-1;
    online_background_t selected=online_background_normalize(s.background);
    const uint8_t *encoded=online_background_data();
    const lv_image_dsc_t *custom=encoded?online_background_image(encoded):NULL;
    if(selected==ONLINE_BACKGROUND_CUSTOM && !custom)selected=ONLINE_BACKGROUND_DAY;
    visibility(background_image,home && !setup && selected!=ONLINE_BACKGROUND_OFF);
    if(displayed_background!=(int)selected && selected!=ONLINE_BACKGROUND_OFF) {
        if(selected==ONLINE_BACKGROUND_CUSTOM) {
            lv_image_set_src(background_image,custom);
            lv_image_set_scale(background_image,256);
        } else {
            lv_image_set_src(background_image,whale_backgrounds[selected]);
            lv_image_set_scale(background_image,256);
        }
        displayed_background=selected;
    }
    visibility(face_layer,home && !setup && !whale);visibility(whale_layer,home && !setup && whale);
    /* Normal phases belong to the character and icons; actionable faults keep
     * their explanation, including audio errors reported in the ready phase. */
    bool detail=!buttons_available || s.feedback_visible || s.phase==ONLINE_ERROR || s.phase==ONLINE_CONNECTING ||
        (s.phase==ONLINE_READY && !s.audio_ok);
    visibility(caption,home && !setup && s.overlay && detail);visibility(setup_panel,home && setup);
    visibility(phase_icon.obj,home && !setup && s.overlay && !detail && (s.phase!=ONLINE_READY || s.mic));
    visibility(home_controls,home && !setup && s.overlay && buttons_available);
    ui_icon_update(&link_icon,UI_LINK,s.linked,s.linked?0x28715E:INK);
    ui_icon_update(&mic_icon,UI_MIC,s.mic || s.ptt_recording,INK);
    ui_icon_update(&action_mic,UI_MIC,s.mic || s.ptt_recording,INK);
    ui_icon_update(&volume_icon,UI_VOLUME_LEVEL,s.volume,INK);
    ui_icon_kind_t phase=s.phase==ONLINE_THINKING?UI_THINKING:
        s.phase==ONLINE_SPEAKING?UI_VOLUME:UI_LISTEN;
    ui_icon_update(&phase_icon,phase,s.phase==ONLINE_THINKING?(int)((now/350)%3):s.phase==ONLINE_SPEAKING?100:s.mic,INK);
    if(home && !setup && whale) {
        static int displayed_frame=-1;
        online_avatar_t frame=online_avatar_frame(s.phase,s.mic || s.ptt_recording,s.level,now);
        if(displayed_frame!=(int)frame) {
            lv_image_set_src(whale_image,whale_avatar_view_source(frame));
            displayed_frame=frame;
        }
    }
    visibility(footer,home && setup);visibility(settings_layer,menu.page==ONLINE_SETTINGS || menu.page==ONLINE_DISPLAY || menu.page==ONLINE_CONNECTION);
    visibility(confirm_layer,menu.page==ONLINE_CONFIRM);
    if(menu.page==ONLINE_SETTINGS || menu.page==ONLINE_DISPLAY || menu.page==ONLINE_CONNECTION) {
        bool display=menu.page==ONLINE_DISPLAY;
        bool connection=menu.page==ONLINE_CONNECTION;
        text_changed(settings_help,connection?"当前状态与操作":s.cues_save_failed?"提示音未保存，请重试":s.overlay_save_failed?"设置未保存，请重试":s.brightness_save_failed?"亮度未保存，请重试":s.background_save_failed?"背景未保存，请重试":s.idle_save_failed?"息屏未保存，请重试":s.theme_save_failed?"界面未保存，请重试":"选择项目，按确定进入");
        text_changed(settings_footer,connection?"上/下翻页 · 确定返回":"上/下选择 · 确定进入");
        visibility(settings_footer,!connection);visibility(configuration,connection);visibility(connection_back,connection);visibility(settings_list,!connection);
        static char status[1024];
        if(connection)snprintf(status,sizeof(status),
            "Wi-Fi：%s\nWi-Fi 密码：%s\n电脑：%s\n配对码：%s\n\n"
            "操作说明\n"
            "首页操作\n"
            "确定：开关麦克风\n"
            "上键：循环音量\n"
            "下键短按：取消\n"
            "按住下键说话，松开发送\n"
            "长按确定：打开设置\n\n"
            "设置操作\n"
            "上/下键：选择项目\n"
            "确定：进入或切换\n"
            "长按确定：返回首页\n"
            "息屏后先按键唤醒\n\n"
            "系统设置\n"
            "切换角色、背景和亮度\n"
            "选择息屏时间\n"
            "界面提示可隐藏图标\n"
            "提示音可统一关闭操作声音\n\n"
            "连接配置\n"
            "状态信息查看配置\n"
            "重新配置需要确认\n"
            "手机连接设备热点\n"
            "浏览器打开 192.168.4.1\n"
            "填写电脑地址和配对码\n\n"
            "上传背景\n"
            "选择上传背景进入热点\n"
            "浏览器打开 192.168.4.1\n"
            "选择自定义背景并上传\n"
            "保存后重启，连接保留",
            s.configured?s.ssid:"未配置",
            !s.configured?"未配置":s.password_set?"已保存":"开放网络",
            s.configured?s.host:"未配置",
            s.configured&&s.token_set?"已保存":"未配置");
        if(connection)text_changed(status_text,status);
        const char *root_choices[]={"系统设置","状态信息","上传背景","返回首页","重新配置"};
        for(unsigned i=0;i<ONLINE_DISPLAY_COUNT;i++)visibility(settings_choices[i],display || i<ONLINE_MENU_COUNT);
        if(!display)for(unsigned i=0;i<ONLINE_MENU_COUNT;i++)text_changed(lv_obj_get_child(settings_choices[i],0),root_choices[i]);
        if(display) {
        text_changed(lv_obj_get_child(settings_choices[ONLINE_DISPLAY_THEME],0),whale?"界面：鲸鱼娘":"界面：薄荷表情");
        const char *background_labels[]={"背景：晴海茶室","背景：星夜鲸梦","背景：自定义","背景：关闭"};
        text_changed(lv_obj_get_child(settings_choices[ONLINE_DISPLAY_BACKGROUND],0),background_labels[selected]);
        lv_label_set_text_fmt(lv_obj_get_child(settings_choices[ONLINE_DISPLAY_BRIGHTNESS],0),"亮度：%u%%",online_brightness_normalize(s.brightness));
        static const char *const idle_labels[]={"息屏：1 分钟","息屏：3 分钟","息屏：10 分钟","息屏：不息屏"};
        text_changed(lv_obj_get_child(settings_choices[ONLINE_DISPLAY_IDLE],0),idle_labels[online_idle_normalize(s.idle)]);
        text_changed(lv_obj_get_child(settings_choices[ONLINE_DISPLAY_OVERLAY],0),s.overlay?"界面提示：开":"界面提示：关");
        text_changed(lv_obj_get_child(settings_choices[ONLINE_DISPLAY_CUE],0),s.cues?"提示音：开":"提示音：关");
        text_changed(lv_obj_get_child(settings_choices[ONLINE_DISPLAY_BACK],0),"返回设置");
        }
    }
    static int settings_selected=-1,settings_page=-1,confirm_selected=-1;
    if((menu.page==ONLINE_SETTINGS || menu.page==ONLINE_DISPLAY) && (settings_selected!=(int)menu.selected || settings_page!=(int)menu.page)) {
        unsigned count=menu.page==ONLINE_DISPLAY?ONLINE_DISPLAY_COUNT:ONLINE_MENU_COUNT;
        for(unsigned i=0;i<count;i++) {
            lv_obj_set_style_bg_color(settings_choices[i],lv_color_hex(menu.selected==i?INK:PAPER),0);
            lv_obj_set_style_text_color(lv_obj_get_child(settings_choices[i],0),lv_color_hex(menu.selected==i?PAPER:INK),0);
        }
        settings_selected=menu.selected;settings_page=menu.page;
        lv_obj_scroll_to_view(settings_choices[menu.selected],LV_ANIM_OFF);
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
    text_changed(caption,!buttons_available?"按键不可用，请检查设备":s.message[0]?s.message:online_phase_text(s.phase));
    text_changed(footer,"长按确定 · 设置");
}
static void update_battery(lv_timer_t *timer) {
    (void)timer;if(atomic_load(&screen_asleep))return;
    int soc=bsp_battery_soc();
    if(soc>100)soc=100;
    ui_icon_update(&battery_icon,UI_BATTERY,soc,soc>=0 && soc<=15?0xA84141:INK);
}
void online_ui_enter(bool buttons_ok) {
    buttons_available=buttons_ok;
    online_background_image_init();
    whale_avatar_view_init();
    lv_obj_t *screen=shape(NULL,0,0,240,320,0xE7F0FF,0);app_screen=screen;
    title=label(screen,"鲸语",64,8,112,20,false,INK);
    background_image=lv_image_create(screen);
    lv_image_set_src(background_image,whale_backgrounds[0]);
    lv_image_set_pivot(background_image,0,0);
    lv_obj_move_background(background_image);
    /* Individual rect blending keeps the photo visible without allocating an
     * off-screen layer; retain enough tint for text over dark custom images. */
    header=shape(screen,0,0,240,35,0xE7F0FF,0);
    lv_obj_set_style_bg_opa(header,LV_OPA_70,0);
    lv_obj_move_to_index(header,1);
    lv_obj_move_foreground(title);
    ui_icon_create(screen,&link_icon,UI_LINK,10,8,20);
    ui_icon_create(screen,&mic_icon,UI_MIC,36,8,20);
    ui_icon_create(screen,&battery_icon,UI_BATTERY,202,6,28);
    face_layer=shape(screen,0,0,240,320,YELLOW,0);
    lv_obj_set_style_bg_opa(face_layer,LV_OPA_TRANSP,0);
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
    whale_layer=shape(screen,0,WHALE_VIEW_TOP,WHALE_VIEW_WIDTH,WHALE_VIEW_HEIGHT,0xE7F0FF,0);
    lv_obj_set_style_bg_opa(whale_layer,LV_OPA_TRANSP,0);
    whale_image=lv_image_create(whale_layer);
    lv_obj_set_pos(whale_image,0,0);
    lv_image_set_src(whale_image,whale_avatar_view_source(ONLINE_AVATAR_IDLE));
    caption=label(screen,"",12,246,216,38,false,INK);
    lv_obj_set_style_bg_color(caption,lv_color_hex(PAPER),0);
    lv_obj_set_style_bg_opa(caption,LV_OPA_60,0);
    lv_obj_set_style_radius(caption,8,0);
    lv_label_set_long_mode(caption,LV_LABEL_LONG_SCROLL_CIRCULAR);
    ui_icon_create(screen,&phase_icon,UI_MIC,178,8,20);
    setup_panel=shape(screen,8,42,224,240,PAPER,14);
    label(setup_panel,"连接设备热点",8,10,208,24,false,INK);
    setup_text=label(setup_panel,"",8,46,208,24,false,INK);
    label(setup_panel,"无需密码，直接连接",8,78,208,24,false,INK);
    label(setup_panel,"浏览器打开",8,116,208,24,false,INK);
    lv_obj_t *address=label(setup_panel,"192.168.4.1",8,148,208,30,false,INK);
    lv_obj_set_style_text_font(address,&lv_font_montserrat_20,0);
    label(setup_panel,"电脑 IP + 4 位配对码",8,196,208,24,false,INK);
    footer=label(screen,"",12,294,216,22,false,INK);
    home_controls=lv_obj_create(screen);lv_obj_remove_style_all(home_controls);
    lv_obj_remove_flag(home_controls,LV_OBJ_FLAG_SCROLLABLE|LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_pos(home_controls,0,284);lv_obj_set_size(home_controls,240,26);
    lv_obj_t *up_hint=key_hint(home_controls,8,0,64,26);
    lv_obj_set_style_bg_color(up_hint,lv_color_hex(0xE7F0FF),0);lv_obj_set_style_bg_opa(up_hint,LV_OPA_60,0);
    ui_icon_create(up_hint,&up_icon,UI_UP,5,3,20);
    ui_icon_create(up_hint,&volume_icon,UI_VOLUME_LEVEL,28,3,20);
    lv_obj_t *ok_hint=key_hint(home_controls,88,0,64,26);
    lv_obj_set_style_bg_color(ok_hint,lv_color_hex(0xE7F0FF),0);lv_obj_set_style_bg_opa(ok_hint,LV_OPA_60,0);
    ui_icon_create(ok_hint,&ok_icon,UI_OK,10,3,20);
    ui_icon_create(ok_hint,&action_mic,UI_MIC,34,3,20);
    lv_obj_t *down_hint=key_hint(home_controls,168,0,64,26);
    lv_obj_set_style_bg_color(down_hint,lv_color_hex(0xE7F0FF),0);lv_obj_set_style_bg_opa(down_hint,LV_OPA_60,0);
    ui_icon_create(down_hint,&down_icon,UI_DOWN,10,3,20);
    ui_icon_create(down_hint,&stop_icon,UI_STOP,34,3,20);
    settings_layer=shape(screen,0,35,240,285,YELLOW,0);
    settings_help=label(settings_layer,"",8,0,224,22,false,INK);
    configuration=shape(settings_layer,8,26,224,210,PAPER,12);
    lv_obj_add_flag(configuration,LV_OBJ_FLAG_SCROLLABLE);lv_obj_set_scroll_dir(configuration,LV_DIR_VER);
    lv_obj_set_scrollbar_mode(configuration,LV_SCROLLBAR_MODE_AUTO);
    status_text=label(configuration,"",8,8,208,0,false,INK);
    lv_obj_set_height(status_text,LV_SIZE_CONTENT);
    connection_back=shape(settings_layer,16,242,208,30,INK,9);
    label(connection_back,"返回设置",4,4,200,22,false,PAPER);
    settings_list=lv_obj_create(settings_layer);lv_obj_remove_style_all(settings_list);
    lv_obj_set_pos(settings_list,0,26);lv_obj_set_size(settings_list,240,230);
    lv_obj_set_scrollbar_mode(settings_list,LV_SCROLLBAR_MODE_OFF);
    const char *choices[]={"系统设置","状态信息","上传背景","返回首页","重新配置","",""};
    for(unsigned i=0;i<ONLINE_DISPLAY_COUNT;i++) {
        settings_choices[i]=shape(settings_list,16,i*28,208,26,PAPER,9);
        label(settings_choices[i],choices[i],4,2,200,22,false,INK);
    }
    settings_footer=label(settings_layer,"上/下选择 · 确定进入",8,262,224,22,false,INK);
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
    static bool wake_gesture,down_home,down_long,up_long;
    if(ev==BSP_BTN_PRESS) {
        if(key==BSP_BTN_UP)up_long=false;
        wake_gesture=atomic_load(&screen_asleep);
        atomic_store(&pending_activity,true);
        online_set_standby(false);
        if(key==BSP_BTN_DOWN) {
            down_long=false;down_home=!wake_gesture && atomic_load(&ptt_home);
            if(down_home)online_ptt_press();
        }
        return;
    }
    if(key==BSP_BTN_DOWN && down_home) {
        if(ev==BSP_BTN_LONG && !wake_gesture) {down_long=true;online_ptt_start();atomic_store(&pending_activity,true);return;}
        if(ev==BSP_BTN_RELEASE) {online_ptt_release();return;}
        if(ev==BSP_BTN_CLICK) {down_home=false;down_long=false;return;}
    }
    if(key==BSP_BTN_DOWN && down_long && ev==BSP_BTN_CLICK) {down_long=false;return;}
    if(key==BSP_BTN_UP && ev==BSP_BTN_LONG)up_long=true;
    if(key==BSP_BTN_UP && ev==BSP_BTN_CLICK && up_long) {up_long=false;return;}
    unsigned command=0;
    if(ev==BSP_BTN_LONG && key==BSP_BTN_OK)command=ONLINE_KEY_LONG;
    else if(ev==BSP_BTN_LONG && key==BSP_BTN_UP)command=ONLINE_KEY_UP_LONG;
    else if(ev==BSP_BTN_CLICK)command=key==BSP_BTN_OK?ONLINE_KEY_OK:key==BSP_BTN_UP?ONLINE_KEY_UP:ONLINE_KEY_DOWN;
    /* Button callbacks never touch LVGL. Consume one action on its task. */
    if(command) {
        if(wake_gesture)return;
        atomic_store(&pending_activity,true);
        if(atomic_load(&screen_asleep))return;
        unsigned empty=0;atomic_compare_exchange_strong(&pending_key,&empty,command);
    }
}
