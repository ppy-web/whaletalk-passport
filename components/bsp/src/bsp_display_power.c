#include "bsp_display.h"
#include "esp_lcd_panel_ops.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static bool sleeping;

esp_err_t bsp_display_sleep(void) {
    esp_lcd_panel_handle_t panel=bsp_display_panel();
    if(!panel)return ESP_ERR_INVALID_STATE;
    if(sleeping)return ESP_OK;
    bsp_display_backlight(0);
    // IDF's SPI parameter transactions drain queued color DMA before the command.
    esp_err_t e=esp_lcd_panel_disp_on_off(panel,false);
    if(e!=ESP_OK)return e;
    e=esp_lcd_panel_disp_sleep(panel,true);
    // A failed transaction may still reach the panel. Always send Sleep Out
    // during recovery, including after a partially completed sleep sequence.
    sleeping=true;
    // IDF 5.5.3's ST7789 driver already waits 100 ms; allow 120 ms in total.
    if(e==ESP_OK)vTaskDelay(pdMS_TO_TICKS(20));
    return e;
}

esp_err_t bsp_display_wake(void) {
    esp_lcd_panel_handle_t panel=bsp_display_panel();
    if(!panel)return ESP_ERR_INVALID_STATE;
    if(!sleeping)return esp_lcd_panel_disp_on_off(panel,true);
    esp_err_t e=esp_lcd_panel_disp_sleep(panel,false);
    if(e!=ESP_OK)return e;
    vTaskDelay(pdMS_TO_TICKS(20));
    e=esp_lcd_panel_disp_on_off(panel,true);
    if(e==ESP_OK)sleeping=false;
    return e;
}
