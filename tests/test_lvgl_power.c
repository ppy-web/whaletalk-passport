/* Exercise the real BSP with a modeled port: stop disables timers even if its
 * tick stop fails, and leaving its task runnable would keep polling at 2 ms.
 * Stub API shapes follow the reference AI Passport SDK's BSP host tests. */
#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include "../components/bsp/src/bsp_display_lvgl.c"

static lv_display_t display;
static lv_timer_t owner_timer;
static int panel_token,io_token,worker_token,caller_token;
static TaskHandle_t current=&caller_token;
static unsigned locks,callbacks,stops,resumes,suspends,task_resumes;
static bool stopped,suspended,fail_stop,fail_resume,fail_timer;
static void (*owner_callback)(lv_timer_t *);
esp_lcd_panel_handle_t bsp_display_panel(void) {return &panel_token;}
esp_lcd_panel_io_handle_t bsp_display_io(void) {return &io_token;}
esp_err_t lvgl_port_init(const lvgl_port_cfg_t *cfg) {(void)cfg;return ESP_OK;}
bool lvgl_port_lock(uint32_t timeout) {(void)timeout;locks++;return true;}
void lvgl_port_unlock(void) {assert(locks);locks--;}
lv_display_t *lvgl_port_add_disp(const lvgl_port_display_cfg_t *cfg) {(void)cfg;assert(locks);return &display;}
esp_err_t lvgl_port_remove_disp(lv_display_t *disp) {(void)disp;return ESP_OK;}
void lv_display_add_event_cb(lv_display_t *disp,void (*callback)(lv_event_t *),int code,void *user) {
    (void)disp;(void)callback;(void)code;(void)user;assert(locks);callbacks++;
}
uint32_t lv_display_get_event_count(lv_display_t *disp) {(void)disp;return callbacks;}
lv_timer_t *lv_timer_create(void (*callback)(lv_timer_t *),uint32_t period,void *user) {
    (void)period;(void)user;assert(locks);
    if(fail_timer)return NULL;
    owner_callback=callback;return &owner_timer;
}
void lv_timer_delete(lv_timer_t *timer) {assert(timer==&owner_timer);}
TaskHandle_t xTaskGetCurrentTaskHandle(void) {return current;}
void vTaskSuspend(TaskHandle_t task) {
    assert(locks && stopped && task==&worker_token && task!=current && !suspended);
    suspended=true;suspends++;
}
void vTaskResume(TaskHandle_t task) {
    assert(locks && !stopped && task==&worker_token && suspended);
    suspended=false;task_resumes++;
}
esp_err_t lvgl_port_stop(void) {
    assert(locks);stopped=true;stops++;return fail_stop?ESP_FAIL:ESP_OK;
}
esp_err_t lvgl_port_resume(void) {
    assert(locks);resumes++;
    if(fail_resume)return ESP_FAIL;
    stopped=false;return ESP_OK;
}
int main(void) {
    assert(bsp_lvgl_pause()==ESP_ERR_INVALID_STATE);
    fail_timer=true;assert(bsp_lvgl_init()==NULL && !locks);
    fail_timer=false;
    assert(bsp_lvgl_init()==&display && owner_callback);
    assert(bsp_lvgl_lock(0));
    // No task-name lookup, and never suspend an unknown or current task.
    assert(bsp_lvgl_pause()==ESP_ERR_INVALID_STATE && !suspends);
    current=&worker_token;owner_callback(&owner_timer);
    assert(bsp_lvgl_pause()==ESP_ERR_INVALID_STATE && !suspends);
    current=&caller_token;
    assert(bsp_lvgl_pause()==ESP_OK && suspended && stopped);
    assert(bsp_lvgl_pause()==ESP_OK && suspends==1 && stops==1);
    fail_resume=true;
    assert(bsp_lvgl_resume()==ESP_FAIL && suspended);
    fail_resume=false;
    assert(bsp_lvgl_resume()==ESP_OK && !suspended && !stopped && task_resumes==1);
    unsigned before=resumes;
    assert(bsp_lvgl_resume()==ESP_OK && resumes==before);
    fail_stop=true;
    assert(bsp_lvgl_pause()==ESP_FAIL && !suspended && !stopped);
    assert(resumes==before+1); // Restore timer handling after partial stop.
    fail_stop=false;
    assert(bsp_lvgl_pause()==ESP_OK && suspended);
    assert(bsp_lvgl_resume()==ESP_OK && !suspended);
    bsp_lvgl_unlock();
    puts("LVGL power: safe task suspension, wake and partial failure recovery PASS");
}
