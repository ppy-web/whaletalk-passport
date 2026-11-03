file(TO_CMAKE_PATH "$ENV{IDF_PATH}" PASSPORT_IDF_PATH)
idf_component_register(
    SRCS "recovery_boot_hook.c"
    PRIV_REQUIRES bootloader_support
    PRIV_INCLUDE_DIRS "${PASSPORT_IDF_PATH}/components/bootloader_support/private_include"
)
