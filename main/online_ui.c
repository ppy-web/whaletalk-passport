#include "online_ui.h"
#include "online_runtime.h"
#include "bsp_battery.h"
#include "bsp_display.h"
#include "esp_timer.h"
#include "lvgl.h"
#include "whale_avatar_view.h"
#include "online_ui_icons.h"
#include "online_background.h"
#include "online_background_image.h"
#include "online_onboarding.h"
#include "whale_backgrounds.h"
#include <stdio.h>
#include <stdatomic.h>
#include <string.h>
LV_FONT_DECLARE(mouthy_bean_zh_12);
LV_FONT_DECLARE(mouthy_bean_zh_16);
#define YELLOW 0xA7E8DC
#define INK 0x173E3A
#define PAPER 0xE9F8F3
#define HELP_QQ "帮助QQ群：1104616387"
typedef struct {
    const char *label;
    uint32_t base,card,border,focus,accent;
} theme_style_t;
static const theme_style_t theme_styles[ONLINE_THEME_COUNT]={
    [ONLINE_THEME_WHALE]={"界面：鲸鱼娘",0xE7F0FF,0xF5FAFF,0xCCDDF0,0xD9EDFF,0x2F6FA7},
    [ONLINE_THEME_MINT]={"界面：薄荷表情",YELLOW,PAPER,0xBADDCF,0xD2F6E8,0x267963},
    [ONLINE_THEME_WHALE_MAID]={"界面：鲸鱼娘2",0xE7F0FF,0xF5FAFF,0xCCDDF0,0xD9EDFF,0x2F6FA7},
    [ONLINE_THEME_CLAUDE]={"界面：Claude",0xFFF0DF,0xFFF8EE,0xECD1B6,0xFBE4CE,0x925325},
};
enum { SETTINGS_ROWS = ONLINE_MENU_COUNT };
_Static_assert(SETTINGS_ROWS>=(int)ONLINE_DISPLAY_COUNT && SETTINGS_ROWS>=(int)ONLINE_SOUND_COUNT &&
               SETTINGS_ROWS>=ONLINE_PROFILE_LIMIT+2,"Settings list cannot fit its longest page");
static const struct {
    const char *label;
    const lv_image_dsc_t *icon;
} settings_cards[ONLINE_MENU_COUNT]={
    [ONLINE_MENU_DISPLAY]={"系统设置",&ui_settings_system_image},
    [ONLINE_MENU_STATUS]={"状态信息",&ui_settings_status_image},
    [ONLINE_MENU_SOUND]={"声音设置",&ui_settings_sound_image},
    [ONLINE_MENU_UPLOAD]={"上传背景",&ui_settings_upload_image},
    [ONLINE_MENU_HOME]={"返回首页",&ui_settings_home_image},
    [ONLINE_MENU_CONNECTION]={"连接设置",&ui_settings_connection_image},
};
static lv_obj_t *face_layer,*pupils[2],*lids[2],*closed_eyes[2],*ears[2],*mouth,*teeth,*tongue,*smile;
static lv_obj_t *title,*header,*footer,*setup_panel,*setup_text;
static lv_obj_t *setup_qr[2];
static lv_obj_t *home_controls,*background_image,*settings_list;
static ui_icon_t link_icon,battery_icon,phase_icon,volume_icon;
static ui_icon_t up_icon,ok_icon,down_icon,action_mic,stop_icon;
static lv_obj_t *app_screen,*portrait_layer,*portrait_image;
static lv_obj_t *settings_layer,*configuration,*settings_footer,*settings_help;
static lv_obj_t *status_value[4],*settings_choices[SETTINGS_ROWS];
static lv_obj_t *bouncing_card;
static int32_t bounce_step;
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
static void draw_settings_icon(lv_event_t *event) {
    if(menu.page!=ONLINE_SETTINGS)return;
    unsigned index=(unsigned)(uintptr_t)lv_event_get_user_data(event);
    lv_obj_t *card=lv_event_get_target(event);
    const lv_image_dsc_t *image=settings_cards[index].icon;
    int32_t bounce=card==bouncing_card?bounce_step:0;
    lv_area_t bounds;
    lv_obj_get_coords(card,&bounds);
    int x=bounds.x1+(lv_area_get_width(&bounds)-image->header.w)/2;
    int y=bounds.y1+26-image->header.h/2-bounce*3/100;
    lv_area_t area={x,y,x+image->header.w-1,y+image->header.h-1};
    lv_draw_image_dsc_t draw;
    lv_draw_image_dsc_init(&draw);
    draw.src=image;
    draw.recolor=lv_obj_get_style_text_color(lv_obj_get_child(card,0),0);
    if(bounce) {
        draw.pivot=(lv_point_t){image->header.w/2,image->header.h/2};
        draw.scale_x=draw.scale_y=LV_SCALE_NONE+bounce*55/100;
        draw.rotation=bounce*13/10;
        draw.antialias=1;
    }
    lv_draw_image(lv_event_get_layer(event),&draw,&area);
}
static void draw_bounce(void *object,int32_t value) {
    bounce_step=value;
    lv_obj_invalidate(object);
}
static void stop_bounce(void) {
    if(!bouncing_card)return;
    lv_anim_delete(bouncing_card,draw_bounce);
    lv_obj_invalidate(bouncing_card);
    bouncing_card=NULL;
    bounce_step=0;
}
static void start_bounce(lv_obj_t *card) {
    stop_bounce();
    bouncing_card=card;
    /* Move only the A8 mask. Keep the card and its focus border fixed. */
    lv_anim_t animation;
    lv_anim_init(&animation);
    lv_anim_set_var(&animation,card);
    lv_anim_set_exec_cb(&animation,draw_bounce);
    lv_anim_set_values(&animation,100,0);
    lv_anim_set_duration(&animation,170);
    lv_anim_set_path_cb(&animation,lv_anim_path_ease_out);
    lv_anim_start(&animation);
}
static void layout_settings_choices(bool grid) {
    lv_obj_scroll_to_y(settings_list,0,LV_ANIM_OFF);
    lv_obj_set_y(settings_list,grid?11:26);
    lv_obj_set_height(settings_list,grid?226:230);
    lv_obj_set_y(settings_help,grid?240:0);
    if(grid)lv_obj_remove_flag(settings_list,LV_OBJ_FLAG_SCROLLABLE);
    else lv_obj_add_flag(settings_list,LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_text_font(settings_footer,grid?&mouthy_bean_zh_12:&mouthy_bean_zh_16,0);
    for(unsigned i=0;i<SETTINGS_ROWS;i++) {
        lv_obj_t *card=settings_choices[i];
        lv_obj_t *caption=lv_obj_get_child(card,0);
        lv_obj_set_pos(card,grid?12+(i%ONLINE_MENU_COLUMNS)*112:16,grid?(i/ONLINE_MENU_COLUMNS)*78:i*28);
        lv_obj_set_size(card,grid?104:208,grid?70:26);
        lv_obj_set_style_radius(card,grid?12:9,0);
        lv_obj_set_style_border_width(card,grid?2:0,0);
        lv_obj_set_pos(caption,4,grid?46:2);
        lv_obj_set_size(caption,grid?92:200,22);
        if(grid && i<ONLINE_MENU_COUNT)text_changed(caption,settings_cards[i].label);
    }
}
static const char *home_status_text(const online_status_t *s) {
    if(!buttons_available)return "按键不可用，请检查设备";
    if(s->phase==ONLINE_SETUP)return "正在配网";
    if(s->feedback_visible && s->message[0])return s->message;
    if(s->phase==ONLINE_CONNECTING || s->phase==ONLINE_ERROR)
        return s->message[0]?s->message:online_phase_text(s->phase);
    if(!s->audio_ok)return "音频不可用，请检查设备";
    if(!s->linked)return "后端未连接";
    if(s->progress && s->progress[0])return s->progress;
    if(s->phase==ONLINE_THINKING)return "正在思考";
    if(s->phase==ONLINE_SPEAKING)return "正在回答";
    return "";
}
static void home_status_update(const char *value) {
    /* Connection errors can contain newlines; keep every message on one line. */
    char line[sizeof(((online_status_t *)0)->message)];size_t used=0;
    for(const char *p=value;*p && used+1<sizeof(line);p++) {
        char c=(*p=='\n' || *p=='\r' || *p=='\t')?' ':*p;
        if(c==' ' && (!used || line[used-1]==' '))continue;
        line[used++]=c;
    }
    if(used && line[used-1]==' ')used--;
    line[used]='\0';
    if(strcmp(lv_label_get_text(title),line)) {
        /* A new warning starts at its beginning, not at the old scroll offset. */
        lv_label_set_long_mode(title,LV_LABEL_LONG_SCROLL);
        lv_label_set_text(title,line);
    }
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
static void setup_codes(const char *ssid) {
    static char rendered[33];
    if(setup_qr[0] && setup_qr[1] && !strcmp(rendered,ssid))return;
    char wifi[84];if(!online_hotspot_qr(ssid,wifi,sizeof(wifi)))return;
    const char *payloads[]={wifi,ONLINE_SETUP_URL};
    for(unsigned i=0;i<2;i++) {
        if(!setup_qr[i]) {
            setup_qr[i]=lv_qrcode_create(setup_panel);
            lv_qrcode_set_size(setup_qr[i],96);
            lv_qrcode_set_quiet_zone(setup_qr[i],true);
            lv_qrcode_set_dark_color(setup_qr[i],lv_color_hex(0x000000));
            lv_qrcode_set_light_color(setup_qr[i],lv_color_hex(0xffffff));
            lv_obj_set_pos(setup_qr[i],8+i*112,90);
        }
        visibility(setup_qr[i],lv_qrcode_update(setup_qr[i],payloads[i],strlen(payloads[i]))==LV_RESULT_OK);
    }
    snprintf(rendered,sizeof(rendered),"%s",ssid);
}
static void update_status_card(const online_status_t *status) {
    char summary[72];
    snprintf(summary,sizeof(summary),"Wi-Fi %s · 后端%s",
        status->has_ip?"已连接":"未连接",status->linked?"已连接":"未连接");
    text_changed(status_value[0],summary);
    text_changed(status_value[1],status->configured?status->ssid:"未配置");
    text_changed(status_value[2],status->configured?status->host:"未配置");
    text_changed(status_value[3],status->configured && status->token_set?"已保存":"未配置");
}
static void refresh(lv_timer_t *timer) {
    (void)timer;
    uint64_t now=esp_timer_get_time()/1000;
    bool activity=atomic_exchange(&pending_activity,false);
    unsigned key=atomic_exchange(&pending_key,0);
    online_status_t s;online_snapshot(&s);
    menu.network_count=s.profile_count;
    menu.provisioning=s.phase==ONLINE_SETUP;
    /* A command queued before the idle deadline still counts as activity. */
    activity=activity || key!=0;
    if(key) {
        switch(online_menu_key(&menu,(online_key_t)key)) {
            case ONLINE_ACTION_MIC: online_toggle_mic();break;
            case ONLINE_ACTION_VOLUME: online_volume();break;
            case ONLINE_ACTION_CANCEL: online_cancel();break;
            case ONLINE_ACTION_SETUP:
                if(s.phase!=ONLINE_SETUP)online_setup();
                break;
            case ONLINE_ACTION_THEME: online_cycle_theme();break;
            case ONLINE_ACTION_IDLE: online_cycle_idle();break;
            case ONLINE_ACTION_BACKGROUND: online_cycle_background();break;
            case ONLINE_ACTION_BRIGHTNESS: online_cycle_brightness();break;
            case ONLINE_ACTION_OVERLAY: online_toggle_overlay();break;
            case ONLINE_ACTION_CUE: online_toggle_cues();break;
            case ONLINE_ACTION_NETWORK: online_select_profile(menu.selected);break;
            case ONLINE_ACTION_RESUME: online_select_profile(0);break;
            default: break;
        }
    }
    if(menu.page!=ONLINE_HOME)online_ptt_abort();
    online_snapshot(&s);
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
    if(asleep) {stop_bounce();return;}
    bool home=menu.page==ONLINE_HOME,setup=s.phase==ONLINE_SETUP;
    atomic_store(&ptt_home,home && !setup);
    bool listen=(s.mic || s.ptt_recording) && (s.phase==ONLINE_LISTENING || s.phase==ONLINE_READY);
    bool speaking=s.phase==ONLINE_SPEAKING;
    online_theme_t theme=online_theme_normalize(s.theme);
    const theme_style_t *style=&theme_styles[theme];
    bool mint=theme==ONLINE_THEME_MINT;
    static int applied_theme=-1;
    if(applied_theme!=(int)theme) {
        lv_obj_set_style_bg_color(app_screen,lv_color_hex(style->base),0);
        lv_obj_set_style_bg_color(settings_layer,lv_color_hex(style->base),0);
        lv_obj_set_style_bg_color(configuration,lv_color_hex(style->card),0);
        applied_theme=theme;
    }
    bool show_overlay=!home || setup || s.overlay;
    static int title_mode=-1;
    int next_title_mode=home?(show_overlay?1:0):2;
    if(title_mode!=next_title_mode) {
        lv_obj_set_style_text_font(title,home?&mouthy_bean_zh_12:&mouthy_bean_zh_16,0);
        lv_obj_set_y(title,home?10:8);
        lv_label_set_long_mode(title,home?(show_overlay?LV_LABEL_LONG_SCROLL:LV_LABEL_LONG_CLIP):LV_LABEL_LONG_WRAP);
        title_mode=next_title_mode;
    }
    if(home) {
        if(show_overlay)home_status_update(home_status_text(&s));
    } else {
        static const char *const pages[]={
            [ONLINE_SETTINGS]="设置",[ONLINE_DISPLAY]="系统设置",[ONLINE_STATUS]="状态信息",
            [ONLINE_SOUND]="声音设置",[ONLINE_CONNECTION]="连接设置",
        };
        text_changed(title,pages[menu.page]);
    }
    visibility(header,show_overlay);visibility(title,show_overlay);
    visibility(link_icon.obj,show_overlay);visibility(battery_icon.obj,show_overlay);
    online_background_t selected=online_background_normalize(s.background);
    const lv_image_dsc_t *background=whale_backgrounds[selected];
    if(selected==ONLINE_BACKGROUND_CUSTOM) {
        background=online_background_image(online_background_data());
        if(!background) {
            selected=ONLINE_BACKGROUND_DAY;
            background=whale_backgrounds[selected];
        }
    }
    visibility(background_image,home && !setup && background);
    if(home && !setup && background && lv_image_get_src(background_image)!=background)
        lv_image_set_src(background_image,background);
    visibility(face_layer,home && !setup && mint);visibility(portrait_layer,home && !setup && !mint);
    /* Actionable details use the top status line; leave room for phase icons. */
    bool detail=!buttons_available || s.feedback_visible || s.phase==ONLINE_ERROR || s.phase==ONLINE_CONNECTING ||
        (s.phase==ONLINE_READY && !s.audio_ok);
    visibility(setup_panel,home && setup);
    visibility(phase_icon.obj,home && !setup && s.overlay && !detail && (s.phase!=ONLINE_READY || s.mic));
    visibility(home_controls,home && !setup && s.overlay && buttons_available);
    ui_icon_update(&link_icon,UI_LINK,s.linked,s.linked?0x28715E:INK);
    ui_icon_update(&action_mic,UI_MIC,s.mic || s.ptt_recording,INK);
    ui_icon_update(&volume_icon,UI_VOLUME_LEVEL,s.volume,INK);
    ui_icon_kind_t phase=s.phase==ONLINE_THINKING?UI_THINKING:
        s.phase==ONLINE_SPEAKING?UI_VOLUME:UI_LISTEN;
    ui_icon_update(&phase_icon,phase,s.phase==ONLINE_THINKING?(int)((now/350)%3):s.phase==ONLINE_SPEAKING?100:s.mic,INK);
    if(home && !setup && !mint) {
        online_avatar_t frame=online_avatar_frame(s.phase,s.mic || s.ptt_recording,s.level,now);
        const lv_image_dsc_t *source=whale_avatar_view_source(theme,frame);
        if(lv_image_get_src(portrait_image)!=source)lv_image_set_src(portrait_image,source);
    }
    visibility(footer,home && setup);visibility(settings_layer,!home);
    if(menu.page!=ONLINE_SETTINGS && bouncing_card)stop_bounce();
    unsigned menu_count=online_menu_items(&menu);
    if(!home) {
        bool grid=menu.page==ONLINE_SETTINGS;
        bool display=menu.page==ONLINE_DISPLAY;
        bool connection=menu.page==ONLINE_CONNECTION;
        bool sound=menu.page==ONLINE_SOUND;
        bool status_page=menu.page==ONLINE_STATUS;
        static bool grid_layout;
        if(grid!=grid_layout) {
            layout_settings_choices(grid);
            grid_layout=grid;
        }
        const char *display_problem=s.overlay_save_failed?"设置未保存，请重试":
            s.brightness_save_failed?"亮度未保存，请重试":
            s.background_save_failed?"背景未保存，请重试":s.idle_save_failed?"息屏未保存，请重试":
            s.theme_save_failed?"界面未保存，请重试":NULL;
        const char *sound_problem=s.volume_save_failed?"音量未保存，请重试":
            s.cues_save_failed?"提示音未保存，请重试":NULL;
        const char *help=NULL;
        const char *controls="上/下选择 · 确定切换";
        if(grid) {
            help=display_problem?display_problem:sound_problem;
            controls="短按逐项 · 长按换行 · 确定进入";
        } else if(display) {
            help=display_problem;
            if(menu.selected==ONLINE_DISPLAY_BACK)controls="确定返回";
        } else if(connection) {
            help=s.profile_save_failed?"切换失败，请重试":
                s.profile_count?"配网历史":"配网历史：暂无记录";
            if(menu.selected==s.profile_count)controls="确定进入配网";
            else if(menu.selected>s.profile_count)controls="确定返回";
            else if(!menu.selected)controls=setup?"确定恢复连接":"上/下选择";
            else controls="确定切换并重启";
        } else if(sound) {
            help=sound_problem;
            if(menu.selected==ONLINE_SOUND_VOLUME)controls="确定调节音量";
            else if(menu.selected==ONLINE_SOUND_BACK)controls="确定返回";
        } else if(status_page) {
            controls="确定返回";
            update_status_card(&s);
        }
        text_changed(settings_help,help?help:"");
        text_changed(settings_footer,controls);
        visibility(settings_help,help!=NULL);
        visibility(configuration,status_page);
        visibility(settings_list,!status_page);
        for(unsigned i=0;i<SETTINGS_ROWS;i++) {
            visibility(settings_choices[i],i<menu_count);
            lv_obj_t *caption=lv_obj_get_child(settings_choices[i],0);
            lv_label_long_mode_t mode=connection?LV_LABEL_LONG_SCROLL:LV_LABEL_LONG_WRAP;
            if(lv_label_get_long_mode(caption)!=mode)lv_label_set_long_mode(caption,mode);
        }
        if(connection) {
            for(unsigned i=0;i<s.profile_count;i++) {
                char name[64];snprintf(name,sizeof(name),i?"%s":"当前：%s",s.profile_ssids[i]);
                text_changed(lv_obj_get_child(settings_choices[i],0),name);
            }
            text_changed(lv_obj_get_child(settings_choices[s.profile_count],0),"重新配置");
            text_changed(lv_obj_get_child(settings_choices[s.profile_count+1],0),"返回设置");
        } else if(sound) {
            char volume[32];
            if(s.volume)snprintf(volume,sizeof(volume),"音量：%u%%",s.volume);
            else snprintf(volume,sizeof(volume),"音量：静音");
            text_changed(lv_obj_get_child(settings_choices[ONLINE_SOUND_VOLUME],0),volume);
            text_changed(lv_obj_get_child(settings_choices[ONLINE_SOUND_CUE],0),s.cues?"提示音：开":"提示音：关");
            text_changed(lv_obj_get_child(settings_choices[ONLINE_SOUND_BACK],0),"返回设置");
        } else if(display) {
            text_changed(lv_obj_get_child(settings_choices[ONLINE_DISPLAY_THEME],0),style->label);
            static const char *const background_labels[ONLINE_BACKGROUND_COUNT]={
                [ONLINE_BACKGROUND_DAY]="背景：晴海茶室",
                [ONLINE_BACKGROUND_NIGHT]="背景：星夜鲸梦",
                [ONLINE_BACKGROUND_SPRING]="背景：春日樱庭",
                [ONLINE_BACKGROUND_SUMMER]="背景：夏日浅滩",
                [ONLINE_BACKGROUND_AUTUMN]="背景：秋枫暖湖",
                [ONLINE_BACKGROUND_WINTER]="背景：冬日雪屋",
                [ONLINE_BACKGROUND_CUSTOM]="背景：自定义",
                [ONLINE_BACKGROUND_OFF]="背景：关闭",
            };
            text_changed(lv_obj_get_child(settings_choices[ONLINE_DISPLAY_BACKGROUND],0),background_labels[selected]);
            char brightness[32];
            snprintf(brightness,sizeof(brightness),"亮度：%u%%",online_brightness_normalize(s.brightness));
            text_changed(lv_obj_get_child(settings_choices[ONLINE_DISPLAY_BRIGHTNESS],0),brightness);
            static const char *const idle_labels[]={"息屏：1 分钟","息屏：3 分钟","息屏：10 分钟","息屏：不息屏"};
            text_changed(lv_obj_get_child(settings_choices[ONLINE_DISPLAY_IDLE],0),idle_labels[online_idle_normalize(s.idle)]);
            text_changed(lv_obj_get_child(settings_choices[ONLINE_DISPLAY_OVERLAY],0),s.overlay?"界面提示：开":"界面提示：关");
            text_changed(lv_obj_get_child(settings_choices[ONLINE_DISPLAY_BACK],0),"返回设置");
        }
    }
    static int settings_selected=-1,settings_page=-1,settings_theme=-1;
    static unsigned previous_count;
    if(menu_count && (settings_selected!=(int)menu.selected || settings_page!=(int)menu.page ||
       settings_theme!=(int)theme || previous_count!=menu_count)) {
        const bool grid=menu.page==ONLINE_SETTINGS;
        for(unsigned i=0;i<menu_count;i++) {
            bool focused=menu.selected==i;
            uint32_t bg=grid?(focused?style->focus:style->card):(focused?INK:PAPER);
            uint32_t fg=grid?(focused?style->accent:INK):(focused?PAPER:INK);
            lv_obj_t *card=settings_choices[i];
            lv_obj_set_style_bg_color(card,lv_color_hex(bg),0);
            lv_obj_set_style_text_color(lv_obj_get_child(card,0),lv_color_hex(fg),0);
            if(grid)lv_obj_set_style_border_color(card,lv_color_hex(focused?style->accent:style->border),0);
        }
        if(grid && (settings_page!=ONLINE_SETTINGS || settings_selected!=(int)menu.selected))
            start_bounce(settings_choices[menu.selected]);
        if(!grid)lv_obj_scroll_to_view(settings_choices[menu.selected],LV_ANIM_OFF);
        settings_selected=menu.selected;settings_theme=theme;previous_count=menu_count;
    }
    settings_page=menu.page;
    if(home && setup) {text_changed(setup_text,s.message);setup_codes(s.message);}
    if(!setup)for(unsigned i=0;i<2;i++)if(setup_qr[i]) {lv_obj_delete(setup_qr[i]);setup_qr[i]=NULL;}
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
    if(home && setup) {
        const char *hint="长按确定 · 设置";
        if(s.profile_count)hint=s.profile_save_failed?"恢复失败 · 确定重试":"确定退出配网 · 长按设置";
        text_changed(footer,hint);
    }
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
    title=label(screen,"",64,8,112,20,false,INK);
    background_image=lv_image_create(screen);
    lv_image_set_src(background_image,whale_backgrounds[ONLINE_BACKGROUND_DAY]);
    lv_image_set_pivot(background_image,0,0);
    lv_obj_move_background(background_image);
    /* Individual rect blending keeps the photo visible without allocating an
     * off-screen layer; retain enough tint for text over dark custom images. */
    header=shape(screen,0,0,240,35,0xE7F0FF,0);
    lv_obj_set_style_bg_opa(header,LV_OPA_70,0);
    lv_obj_move_to_index(header,1);
    lv_obj_move_foreground(title);
    ui_icon_create(screen,&link_icon,UI_LINK,10,8,20);
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
    portrait_layer=shape(screen,0,WHALE_VIEW_TOP,WHALE_VIEW_WIDTH,WHALE_VIEW_HEIGHT,0xE7F0FF,0);
    lv_obj_set_style_bg_opa(portrait_layer,LV_OPA_TRANSP,0);
    portrait_image=lv_image_create(portrait_layer);
    lv_obj_set_pos(portrait_image,0,0);
    ui_icon_create(screen,&phase_icon,UI_MIC,178,8,20);
    setup_panel=shape(screen,8,42,224,248,PAPER,14);
    label(setup_panel,"手机扫码配网",8,8,208,24,false,INK);
    setup_text=label(setup_panel,"",8,34,208,24,false,INK);
    label(setup_panel,"1. 连热点",8,62,96,22,false,INK);
    label(setup_panel,"2. 打开网页",120,62,96,22,false,INK);
    lv_obj_t *address=label(setup_panel,"192.168.4.1",8,191,208,25,false,INK);
    lv_obj_set_style_text_font(address,&lv_font_montserrat_20,0);
    label(setup_panel,"热点无需密码",8,218,208,15,true,INK);
    label(setup_panel,HELP_QQ,8,233,208,15,true,INK);
    footer=label(screen,"长按确定进入设置",12,294,216,22,false,INK);
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
    configuration=shape(settings_layer,12,12,216,194,PAPER,12);
    status_value[0]=label(configuration,"",12,13,192,19,true,INK);
    shape(configuration,12,40,192,1,0xAACCC3,0);
    const char *fields[]={"Wi-Fi","电脑","配对码"};
    for(unsigned i=0;i<3;i++) {
        lv_obj_t *name=label(configuration,fields[i],12,51+i*33,44,22,true,INK);
        lv_obj_set_style_text_align(name,LV_TEXT_ALIGN_LEFT,0);
        status_value[i+1]=label(configuration,"",59,50+i*33,145,22,false,INK);
        lv_obj_set_style_text_align(status_value[i+1],LV_TEXT_ALIGN_LEFT,0);
        lv_label_set_long_mode(status_value[i+1],LV_LABEL_LONG_SCROLL);
    }
    shape(configuration,12,150,192,1,0xAACCC3,0);
    label(configuration,HELP_QQ,12,164,192,20,false,INK);
    settings_list=lv_obj_create(settings_layer);lv_obj_remove_style_all(settings_list);
    lv_obj_set_pos(settings_list,0,26);lv_obj_set_size(settings_list,240,230);
    lv_obj_set_scrollbar_mode(settings_list,LV_SCROLLBAR_MODE_OFF);
    for(unsigned i=0;i<SETTINGS_ROWS;i++) {
        settings_choices[i]=shape(settings_list,16,i*28,208,26,PAPER,9);
        label(settings_choices[i],"",4,2,200,22,false,INK);
        if(i<ONLINE_MENU_COUNT)
            lv_obj_add_event_cb(settings_choices[i],draw_settings_icon,LV_EVENT_DRAW_MAIN,(void *)(uintptr_t)i);
    }
    settings_footer=label(settings_layer,"上/下选择 · 确定进入",8,262,224,22,false,INK);
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
    else if(ev==BSP_BTN_LONG && key==BSP_BTN_DOWN) {down_long=true;command=ONLINE_KEY_DOWN_LONG;}
    else if(ev==BSP_BTN_CLICK)command=key==BSP_BTN_OK?ONLINE_KEY_OK:key==BSP_BTN_UP?ONLINE_KEY_UP:ONLINE_KEY_DOWN;
    /* Button callbacks never touch LVGL. Consume one action on its task. */
    if(command) {
        if(wake_gesture)return;
        atomic_store(&pending_activity,true);
        if(atomic_load(&screen_asleep))return;
        unsigned empty=0;atomic_compare_exchange_strong(&pending_key,&empty,command);
    }
}
