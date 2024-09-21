set(SMARTAQ_SOURCES
            "app_main.cpp"
            "main_thread.h" "main_thread.cpp"
            "actions/device_actions.h" "actions/device_actions.cpp"
            "actions/stats_actions.h" "actions/stats_actions.cpp"
            "rest/devices_rest.h" "rest/devices_rest.cpp"
            "rest/stats_rest.h" "rest/stats_rest.cpp"
            "drivers/driver_interface.h"
            "drivers/bme280_driver.h"
            "drivers/device_resource.h" "drivers/device_resource.cpp"
            "drivers/devices.h" "drivers/devices.cpp"
            "drivers/pin_driver.h" "drivers/pin_driver.cpp"
            "drivers/dac_driver.h" "drivers/dac_driver.cpp"
            "drivers/dhtxx_driver.h" "drivers/dhtxx_driver.cpp"
            "drivers/ds18x20_driver.h" "drivers/ds18x20_driver.cpp"
            "drivers/ads111x_driver.h" "drivers/ads111x_driver.cpp"
            "drivers/pcf8575_driver.h" "drivers/pcf8575_driver.cpp"
            "drivers/scale_driver.h" "drivers/scale_driver.cpp"
            "drivers/schedule_driver.h" "drivers/schedule_driver.cpp"
            "drivers/stepper_dosing_pump_driver.h" "drivers/stepper_dosing_pump_driver.cpp"
            "drivers/drv8825_driver.h" "drivers/drv8825_driver.cpp"
            "drivers/ph_probe_driver.h" "drivers/ph_probe_driver.cpp"
            "drivers/switch_driver.h" "drivers/switch_driver.cpp"
            "drivers/rmt_stepper_driver.h" "drivers/rmt_stepper_driver.cpp"
            "drivers/pico_device_driver.h" "drivers/pico_device_driver.cpp"
            "drivers/bme280_driver.h" "drivers/ bme280_driver.cpp"
            "utils/idf_utils.cpp"
            # "storage/sd_filesystem.cpp"
            "utils/filesystem_utils.cpp"
            "utils/time_utils.cpp"
            "utils/logger.cpp"
            "utils/web_utils.cpp"
            "utils/container/lookup_table.h"
            "utils/call_on_destructor.h"
            "../external_libs/frozen/frozen.c")

set(SMARTAQ_PLATFORM_INDEPENDENT_LIB_SOURCES
    utils/container/event_access_array.h
    utils/container/lookup_table.h
    utils/container/ring_buffer.h
    utils/container/sample_container.h
)

idf_component_register(
        SRCS ${SMARTAQ_SOURCES}
        INCLUDE_DIRS
            "."
            "../external_libs/frozen"
            "../header_only/frozen/include"
            "../header_only/ctre/single-header"
        REQUIRES
            esp_driver_ledc
            esp_driver_rmt
            esp_driver_sdmmc
            esp_driver_sdspi
            nvs_flash
            spi_flash
            esp_http_server
            esp_http_client
            esp_wifi
            fatfs
            mbedtls
            wpa_supplicant
        # esp-idf-lib
            ads111x
            dht
            ds18x20
            hx711
            bmp280
            i2cdev
            pcf8575
)

target_link_libraries(${COMPONENT_LIB} PRIVATE pico_driver_i2c)

add_custom_target(deletebloat ALL
        COMMAND rm ${CMAKE_CURRENT_SOURCE_DIR}/../gui_smart_aq/build/*.map
        || true)
add_library(smartaq_lib INTERFACE
    ${SMARTAQ_PLATFORM_INDEPENDENT_LIB_SOURCES})
target_include_directories(smartaq_lib INTERFACE .)


# spiffs_create_partition_image(storage ../gui_smart_aq/build/ FLASH_IN_PROJECT)
# fatfs_create_spiflash_image(values ../values FLASH_IN_PROJECT)