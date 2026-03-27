set(MATTER_EXAMPLES_COMMON_DIR "$ENV{ESP_MATTER_PATH}/connectedhomeip/connectedhomeip/examples/platform/esp32/common")

idf_component_register(
    SRC_DIRS        "." "drivers"
    PRIV_INCLUDE_DIRS "." "drivers" "${MATTER_EXAMPLES_COMMON_DIR}"
    PRIV_REQUIRES   esp_matter
    WHOLE_ARCHIVE
)

target_compile_options(${COMPONENT_LIB} PRIVATE "-DCHIP_HAVE_CONFIG_H")
