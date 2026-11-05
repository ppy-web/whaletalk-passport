#pragma once
#include "lvgl.h"

/* Small vector indicators: no bitmap buffers, font additions or child objects.
 * Coordinates use a 24-unit grid and are scaled for the 240x320 display. */
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
static void ui_rect(ui_pen_t *p,int x,int y,int w,int h,int radius,bool fill) {
    lv_draw_rect_dsc_t d;lv_draw_rect_dsc_init(&d);
    d.radius=ui_px(p,radius);d.bg_color=p->color;d.border_color=p->color;
    d.bg_opa=fill?LV_OPA_COVER:LV_OPA_TRANSP;
    d.border_opa=fill?LV_OPA_TRANSP:LV_OPA_COVER;d.border_width=ui_px(p,2);
    lv_area_t a={p->x+ui_px(p,x),p->y+ui_px(p,y),
        p->x+ui_px(p,x+w)-1,p->y+ui_px(p,y+h)-1};
    lv_draw_rect(p->layer,&d,&a);
}
static void ui_arc(ui_pen_t *p,int x,int y,int radius,int from,int to) {
    lv_draw_arc_dsc_t d;lv_draw_arc_dsc_init(&d);
    d.center=(lv_point_t){p->x+ui_px(p,x),p->y+ui_px(p,y)};
    d.radius=ui_px(p,radius);d.width=ui_px(p,2);d.color=p->color;
    d.start_angle=from;d.end_angle=to;d.rounded=1;lv_draw_arc(p->layer,&d);
}
static void ui_draw_icon(lv_event_t *event) {
    ui_icon_t *i=lv_event_get_user_data(event);lv_area_t a;lv_obj_get_coords(i->obj,&a);
    ui_pen_t p={lv_event_get_layer(event),a.x1,a.y1,lv_obj_get_height(i->obj),lv_color_hex(i->color)};
    switch(i->kind) {
        case UI_LINK:
            ui_arc(&p,8,15,6,45,255);ui_arc(&p,16,9,6,225,435);
            if(i->value)ui_line(&p,8,16,16,8);
            else {ui_line(&p,3,3,7,7);ui_line(&p,17,17,21,21);}
            break;
        case UI_MIC:
            ui_rect(&p,9,3,6,12,3,false);ui_arc(&p,12,11,9,0,180);
            ui_line(&p,12,20,12,22);ui_line(&p,8,22,16,22);
            if(!i->value)ui_line(&p,3,3,21,21);
            break;
        case UI_BATTERY: {
            ui_rect(&p,1,6,19,12,3,false);ui_rect(&p,21,10,2,4,1,true);
            if(i->value<0)ui_line(&p,7,12,13,12);
            else if(i->value>0) {
                int width=(i->value*13+99)/100;
                ui_rect(&p,4,9,width,6,1,true);
            }
            break;
        }
        case UI_VOLUME:
            ui_line(&p,3,9,7,9);ui_line(&p,7,9,12,5);ui_line(&p,12,5,12,19);
            ui_line(&p,12,19,7,15);ui_line(&p,7,15,3,15);ui_line(&p,3,15,3,9);
            if(i->value) {
                ui_arc(&p,12,12,6,310,410);
                if(i->value>50)ui_arc(&p,12,12,10,310,410);
            } else {ui_line(&p,17,9,22,15);ui_line(&p,17,15,22,9);}
            break;
        case UI_UP: case UI_DOWN: {
            int top=i->kind==UI_UP?7:17,bottom=24-top;
            ui_line(&p,6,bottom,12,top);ui_line(&p,12,top,18,bottom);
            break;
        }
        case UI_OK:
            ui_arc(&p,12,12,10,0,360);
            ui_line(&p,7,12,11,16);ui_line(&p,11,16,17,9);break;
        case UI_STOP:ui_rect(&p,5,5,14,14,3,false);break;
        case UI_THINKING:
            for(int n=0;n<3;n++)ui_rect(&p,3+n*8,n==i->value?7:11,3,n==i->value?6:3,2,true);
            break;
        case UI_LISTEN:
            ui_line(&p,4,10,4,14);ui_line(&p,8,6,8,18);ui_line(&p,12,3,12,21);
            ui_line(&p,16,6,16,18);ui_line(&p,20,10,20,14);break;
        case UI_VOLUME_LEVEL: {
            /* Speaker and level bars form one volume indicator. */
            ui_line(&p,2,9,6,9);ui_line(&p,6,9,10,5);ui_line(&p,10,5,10,19);
            ui_line(&p,10,19,6,15);ui_line(&p,6,15,2,15);ui_line(&p,2,15,2,9);
            static const int steps[]={25,50,75,90,100};
            if(!i->value) {
                ui_line(&p,17,8,26,17);ui_line(&p,17,17,26,8);
            } else for(int n=0;n<5;n++) {
                bool active=i->value>=steps[n];
                int height=active?4+n*2:2;
                ui_rect(&p,15+n*4,18-height,2,height,1,true);
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
