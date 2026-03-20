set(SMARTAQ_SOURCES
            "app_main.cpp"
            "main_thread.h" "main_thread.cpp"
            "actions/device_actions.h" "actions/device_actions.cpp"
            "actions/stats_actions.h" "actions/stats_actions.cpp"
            "rest/devices_rest.h" "rest/devices_rest.cpp"
            "rest/settings_rest.h" "rest/settings_rest.cpp"
            "rest/stats_rest.h" "rest/stats_rest.cpp"

            "drivers/hal/esp/esp_remote_driver.h"

            "drivers/infrared_driver.h" "drivers/infrared_driver.cpp"
            "drivers/bme280_driver.h"
            "drivers/device_resource.h" "drivers/device_resource.cpp"
            "drivers/devices.h" "drivers/devices.cpp"
            "drivers/pin_driver.h" "drivers/pin_driver.cpp"
            "drivers/dac_driver.h" "drivers/dac_driver.cpp"
            "drivers/dhtxx_driver.h" "drivers/dhtxx_driver.cpp"
            "drivers/ds18x20_driver.h" "drivers/ds18x20_driver.cpp"
            "drivers/ads111x_driver.h" "drivers/ads111x_driver.cpp"
            "drivers/pcf857x_driver.h" "drivers/pcf857x_driver.cpp"
            "drivers/scale_driver.h" "drivers/scale_driver.cpp"
            "drivers/schedule_driver.h" "drivers/schedule_driver.cpp"
            "drivers/dosing_pump_driver.h" "drivers/dosing_pump_driver.cpp"
            "drivers/drv8825_driver.h" "drivers/drv8825_driver.cpp"
            "drivers/ph_probe_driver.h" "drivers/ph_probe_driver.cpp"
            "drivers/switch_driver.h"
            "drivers/sensor_combiner.h" "drivers/sensor_combiner.cpp"
            "drivers/rmt_stepper_driver.h" "drivers/rmt_stepper_driver.cpp"
            "drivers/pico_device_driver.h" "drivers/pico_device_driver.cpp"
            "drivers/bme280_driver.h" "drivers/bme280_driver.cpp"
            "utils/esp/idf_utils.h" "utils/esp/idf_utils.cpp"
            "utils/esp/thread_utils.h"
            "utils/esp/web_utils.h" "utils/esp/web_utils.cpp"
            "storage/sd_filesystem.cpp"
            "utils/esp/esp_filesystem_utils.h"
            "utils/logger.cpp"
            "../external_libs/frozen/frozen.c")

set(SMARTAQ_PLATFORM_INDEPENDENT_LIB_SOURCES
    drivers/hal/remote_driver.h
    drivers/hal/device_config.h
    drivers/i2c_address_value.h
    drivers/device_types.h
    drivers/sensor_driver_interface.h
    drivers/device_state.h

    utils/container/event_access_array.h
    utils/container/lookup_table.h
    utils/container/ring_buffer.h
    utils/container/sample_container.h
    utils/container/bitset.h

    utils/time/schedule.h
    utils/time/schedule_tracker.h
    utils/time/time_utils.h utils/time/time_utils.cpp

    utils/filesystem_utils.h utils/filesystem_utils.cpp
    utils/memory_helper.h
    utils/bit_utils.h
    utils/buffer_types.h

    utils/serialization/number_conversion.h
    utils/serialization/json_utils.h utils/serialization/json_utils.cpp
    utils/serialization/device_values_serialization.h
    utils/serialization/serialized_representation.h
    utils/serialization/serializer.h
    utils/device_values.h utils/device_values.cpp
    utils/conditional_thread_safety.h
    utils/variant_utils.h

    utils/type/type_helper.h
    utils/type/type_list.h
    utils/type/unique_type_list.h
    utils/type/type_utils.h
    utils/type/enum_sequence.h
)

if (NOT "$ENV{IDF_PATH}" STREQUAL "")
    idf_component_register(
            SRCS ${SMARTAQ_SOURCES}
            INCLUDE_DIRS
                "."
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
                pcf8574
    )

    target_link_libraries(${COMPONENT_LIB} PRIVATE pico_driver_i2c)
endif()

add_custom_target(deletebloat ALL
        COMMAND rm ${CMAKE_CURRENT_SOURCE_DIR}/../gui_smart_aq/build/*.map
        || true)
add_library(smartaq_lib STATIC
    ${SMARTAQ_PLATFORM_INDEPENDENT_LIB_SOURCES})
target_include_directories(smartaq_lib PUBLIC .)
target_include_directories(smartaq_lib PUBLIC "../header_only/ctre/single-header")
target_include_directories(smartaq_lib PUBLIC "../header_only/frozen/include")
target_include_directories(smartaq_lib PUBLIC "../external_libs/frozen")
target_compile_options(smartaq_lib PUBLIC "-fconstexpr-ops-limit=83554432")
target_sources(smartaq_lib PUBLIC "../external_libs/frozen/frozen.c")

if (TARGET ${COMPONENT_LIB})
    target_link_libraries(${COMPONENT_LIB} PRIVATE smartaq_lib)
endif()

# spiffs_create_partition_image(storage ../gui_smart_aq/build/ FLASH_IN_PROJECT)
# fatfs_create_spiflash_image(values ../values FLASH_IN_PROJECT)