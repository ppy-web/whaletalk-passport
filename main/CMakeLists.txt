idf_component_register(
    SRCS "main.c" "online_ui.c" "online_runtime.c" "online_state.c" "online_cue.c" "online_input.c"
         "online_setup.c" "online_setup_scan.c" "online_profiles.c" "online_onboarding.c" "online_theme.c" "online_background.c"
         "online_background_format.c" "online_background_image.c" "whale_avatar_view.c" "fap_screenshot.c"
         "../assets/characters/whale_avatar.c" "../assets/characters/whale_maid_avatar.c" "../assets/characters/claude_avatar.c"
         "../assets/backgrounds/whale_backgrounds.c"
         "../assets/fonts/mouthy_bean_zh_12.c" "../assets/fonts/mouthy_bean_zh_16.c"
    INCLUDE_DIRS "."
    EMBED_TXTFILES "setup-page.html" "appearance-page.html" "background-image.js"
    REQUIRES bsp driver esp_hw_support esp_wifi esp_event esp_netif nvs_flash
             esp_http_server esp_websocket_client json mbedtls esp_timer)
target_add_binary_data(${COMPONENT_LIB} "../assets/backgrounds/seaside-tea.png" BINARY)
target_add_binary_data(${COMPONENT_LIB} "../assets/backgrounds/moonlit-dream.png" BINARY)
