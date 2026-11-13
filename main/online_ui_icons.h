#pragma once
#include "lvgl.h"
#include "online_ui_icon_data.h"

/* Pre-rendered A8 masks are read directly from flash; no full image buffers or
 * child objects are needed. Dynamic levels use the same 24-unit grid. */
typedef enum {
    UI_LINK, UI_MIC, UI_BATTERY, UI_VOLUME, UI_UP, UI_DOWN, UI_OK,
    UI_STOP, UI_THINKING, UI_LISTEN, UI_VOLUME_LEVEL
} ui_icon_kind_t;
typedef struct {
    lv_obj_t *obj;
    ui_icon_kind_t kind;
    int value;
    uint32_t color;
} ui_icon_t;
typedef struct { lv_layer_t *layer; int x,y,size; lv_color_t color; } ui_pen_t;
static int ui_px(const ui_pen_t *p,int n) { return (n*p->size+12)/24; }
static void ui_line(ui_pen_t *p,int x1,int y1,int x2,int y2) {
    lv_draw_line_dsc_t d;lv_draw_line_dsc_init(&d);
    d.p1=(lv_point_precise_t){p->x+ui_px(p,x1),p->y+ui_px(p,y1)};
    d.p2=(lv_point_precise_t){p->x+ui_px(p,x2),p->y+ui_px(p,y2)};
    d.color=p->color;d.width=ui_px(p,2);d.round_start=1;d.round_end=1;
    lv_draw_line(p->layer,&d);
}
static void ui_rect(ui_pen_t *p,int x,int y,int w,int h,int radius,bool fill,lv_opa_t opacity) {
    lv_draw_rect_dsc_t d;lv_draw_rect_dsc_init(&d);
    d.radius=ui_px(p,radius);d.bg_color=p->color;d.border_color=p->color;
    d.bg_opa=fill?opacity:LV_OPA_TRANSP;
    d.border_opa=fill?LV_OPA_TRANSP:opacity;d.border_width=ui_px(p,2);
    lv_area_t a={p->x+ui_px(p,x),p->y+ui_px(p,y),
        p->x+ui_px(p,x+w)-1,p->y+ui_px(p,y+h)-1};
    lv_draw_rect(p->layer,&d,&a);
}
static void ui_image(ui_pen_t *p,const lv_image_dsc_t *image) {
    lv_draw_image_dsc_t d;lv_draw_image_dsc_init(&d);
    d.src=image;d.recolor=p->color;
    lv_area_t a={p->x,p->y,p->x+image->header.w-1,p->y+image->header.h-1};
    lv_draw_image(p->layer,&d,&a);
}
static void ui_draw_icon(lv_event_t *event) {
    ui_icon_t *i=lv_event_get_user_data(event);lv_area_t a;lv_obj_get_coords(i->obj,&a);
    ui_pen_t p={lv_event_get_layer(event),a.x1,a.y1,lv_obj_get_height(i->obj),lv_color_hex(i->color)};
    switch(i->kind) {
        case UI_LINK:ui_image(&p,i->value?&ui_link_on_image:&ui_link_off_image);break;
        case UI_MIC:ui_image(&p,i->value?&ui_mic_on_image:&ui_mic_off_image);break;
        case UI_BATTERY: {
            ui_image(&p,&ui_battery_image);
            if(i->value<0)ui_line(&p,7,12,13,12);
            else if(i->value>0) {
                int width=(i->value*13+99)/100;
                ui_rect(&p,4,9,width,6,1,true,LV_OPA_COVER);
            }
            break;
        }
        case UI_VOLUME:ui_image(&p,i->value<=0?&ui_volume_off_image:
            i->value<=50?&ui_volume_low_image:&ui_volume_high_image);break;
        case UI_UP:ui_image(&p,&ui_up_image);break;
        case UI_DOWN:ui_image(&p,&ui_down_image);break;
        case UI_OK:ui_image(&p,&ui_ok_image);break;
        case UI_STOP:ui_image(&p,&ui_stop_image);break;
        case UI_THINKING:ui_image(&p,i->value==0?&ui_thinking_0_image:
            i->value==1?&ui_thinking_1_image:&ui_thinking_2_image);break;
        case UI_LISTEN:ui_image(&p,&ui_listen_image);break;
        case UI_VOLUME_LEVEL: {
            ui_image(&p,&ui_volume_level_image);
            static const int steps[]={25,50,75,90,100};
            if(!i->value) {
                ui_line(&p,17,9,25,17);ui_line(&p,17,17,25,9);
            } else for(int n=0;n<5;n++) {
                bool active=i->value>=steps[n];
                int height=4+n*2;
                ui_rect(&p,15+n*4,18-height,2,height,1,true,active?LV_OPA_COVER:LV_OPA_30);
            }
            break;
        }
    }
}
static void ui_icon_create(lv_obj_t *parent,ui_icon_t *i,ui_icon_kind_t kind,int x,int y,int size) {
    i->obj=lv_obj_create(parent);lv_obj_remove_style_all(i->obj);
    lv_obj_remove_flag(i->obj,LV_OBJ_FLAG_SCROLLABLE|LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_pos(i->obj,x,y);lv_obj_set_size(i->obj,size,size);
    if(kind==UI_VOLUME_LEVEL)lv_obj_set_width(i->obj,(size*34+23)/24);
    i->kind=kind;i->value=0;i->color=0x173E3A;
    lv_obj_add_event_cb(i->obj,ui_draw_icon,LV_EVENT_DRAW_MAIN,i);
}
static void ui_icon_update(ui_icon_t *i,ui_icon_kind_t kind,int value,uint32_t color) {
    if(i->kind==kind && i->value==value && i->color==color)return;
    i->kind=kind;i->value=value;i->color=color;lv_obj_invalidate(i->obj);
}
