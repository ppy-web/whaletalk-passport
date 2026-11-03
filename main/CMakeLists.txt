idf_component_register(
    SRCS "main.c" "online_ui.c" "online_runtime.c" "online_state.c"
         "online_setup.c" "online_theme.c" "fap_screenshot.c"
         "../assets/characters/whale_avatar.c"
         "../assets/fonts/mouthy_bean_zh_12.c" "../assets/fonts/mouthy_bean_zh_16.c"
    INCLUDE_DIRS "."
    EMBED_TXTFILES "setup-page.html"
    REQUIRES bsp driver esp_hw_support esp_wifi esp_event esp_netif nvs_flash
             esp_http_server esp_websocket_client json mbedtls esp_timer)
