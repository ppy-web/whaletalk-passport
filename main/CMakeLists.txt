set(portal_assets "setup-page.html" "appearance-page.html" "background-image.js")
set(portal_gzip_files)
foreach(asset IN LISTS portal_assets)
    list(APPEND portal_gzip_files "${CMAKE_CURRENT_BINARY_DIR}/${asset}.gz")
endforeach()

idf_component_register(
    SRCS "main.c" "online_ui.c" "online_runtime.c" "online_state.c" "online_cue.c" "online_input.c"
         "online_setup.c" "online_setup_scan.c" "online_profiles.c" "online_onboarding.c" "online_theme.c" "online_background.c"
         "online_background_format.c" "online_background_image.c" "whale_avatar_view.c" "whale_band.c" "fap_screenshot.c"
         "../assets/characters/whale_avatar.c" "../assets/characters/whale_maid_avatar.c" "../assets/characters/claude_avatar.c"
         "../assets/characters/chatgpt_avatar.c"
         "../assets/backgrounds/whale_backgrounds.c"
         "../assets/ui/home_foreground.c" "../assets/ui/claude_foreground.c" "../assets/ui/chatgpt_foreground.c"
         "../assets/fonts/mouthy_bean_zh_12.c" "../assets/fonts/mouthy_bean_zh_16.c"
         "../assets/fonts/whaletalk_ssid_symbols.c"
         "../assets/fonts/whaletalk_address_20.c"
    INCLUDE_DIRS "."
    EMBED_FILES ${portal_gzip_files}
    REQUIRES bsp driver esp_hw_support esp_wifi esp_event esp_netif nvs_flash
             esp_http_server esp_websocket_client json mbedtls esp_timer)

idf_build_get_property(python PYTHON)
set(gzip_script "${CMAKE_CURRENT_SOURCE_DIR}/../tools/gzip_web_assets.py")
foreach(asset IN LISTS portal_assets)
    set(source "${CMAKE_CURRENT_SOURCE_DIR}/${asset}")
    set(output "${CMAKE_CURRENT_BINARY_DIR}/${asset}.gz")
    add_custom_command(OUTPUT "${output}"
        COMMAND "${python}" "${gzip_script}" "${source}" "${output}"
        DEPENDS "${source}" "${gzip_script}"
        VERBATIM)
endforeach()
