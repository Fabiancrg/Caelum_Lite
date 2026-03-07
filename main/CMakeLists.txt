idf_component_register(
    SRC_DIRS  "." "/home/fabian/esp/v5.5.3/esp-idf/examples/zigbee/zb_common_components/examples_utils"
    INCLUDE_DIRS "." "/home/fabian/esp/v5.5.3/esp-idf/examples/zigbee/zb_common_components/examples_utils/include"
    PRIV_REQUIRES nvs_flash esp_driver_uart ieee802154 app_update esp_adc esp_timer
)

# Make generated build-time header visible to this component
target_include_directories(${COMPONENT_LIB} PRIVATE "${CMAKE_BINARY_DIR}/generated")
