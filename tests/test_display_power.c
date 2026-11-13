#include "bsp_display.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static int panel_token;
static bool present,backlight_on=true;
static char events[32];
static unsigned count,fail_at,delay_ms;
esp_lcd_panel_handle_t bsp_display_panel(void) {return present?&panel_token:NULL;}
void bsp_display_backlight(uint8_t percent) {backlight_on=percent!=0;}
static esp_err_t command(char event) {
    assert(count<sizeof(events));events[count++]=event;
    return count==fail_at?ESP_FAIL:ESP_OK;
}
esp_err_t esp_lcd_panel_disp_on_off(esp_lcd_panel_handle_t panel,bool on) {
    assert(panel==&panel_token);return command(on?'O':'o');
}
esp_err_t esp_lcd_panel_disp_sleep(esp_lcd_panel_handle_t panel,bool asleep) {
    assert(panel==&panel_token);
    esp_err_t e=command(asleep?'S':'W');
    if(e==ESP_OK)delay_ms+=100; // Pinned IDF ST7789 driver's own delay.
    return e;
}
void vTaskDelay(uint32_t ticks) {delay_ms+=ticks;}
static void reset_transport(void) {count=fail_at=delay_ms=0;memset(events,0,sizeof(events));}
int main(void) {
    assert(bsp_display_sleep()==ESP_ERR_INVALID_STATE);
    assert(bsp_display_wake()==ESP_ERR_INVALID_STATE);
    present=true;
    assert(bsp_display_sleep()==ESP_OK && !backlight_on);
    assert(count==2 && !memcmp(events,"oS",2) && delay_ms>=120);
    assert(bsp_display_sleep()==ESP_OK && count==2);
    delay_ms=0;
    assert(bsp_display_wake()==ESP_OK && !backlight_on);
    assert(count==4 && !memcmp(events+2,"WO",2) && delay_ms>=120);
    // Failure of either entry command must remain recoverable.
    for(unsigned fail=1;fail<=2;fail++) {
        reset_transport();fail_at=fail;
        assert(bsp_display_sleep()==ESP_FAIL && !backlight_on);
        reset_transport();
        assert(bsp_display_wake()==ESP_OK);
        assert(events[count-1]=='O' && !backlight_on);
        if(fail==2)assert(count==2 && events[0]=='W');
    }
    // Both wake boundaries retain enough state to retry the full sequence.
    for(unsigned fail=1;fail<=2;fail++) {
        reset_transport();assert(bsp_display_sleep()==ESP_OK);
        reset_transport();fail_at=fail;
        assert(bsp_display_wake()==ESP_FAIL && !backlight_on);
        reset_transport();assert(bsp_display_wake()==ESP_OK);
        assert(count==2 && !memcmp(events,"WO",2));
    }
    puts("Panel power: sleep, wake, ordering and partial failure recovery PASS");
}
