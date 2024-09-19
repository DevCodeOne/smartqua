set(SOURCES "aq_main.cpp"
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

set(PLATFORM_INDEPENDENT_LIB_SOURCES
    utils/container/event_access_array.h
    utils/container/lookup_table.h
    utils/container/ring_buffer.h
    utils/container/sample_container.h
        ../tests/ring_buffer_tests.cpp)

if (NOT "$ENV{IDF_PATH}" STREQUAL "")
    idf_component_register(SRCS ${SOURCES}
            INCLUDE_DIRS
            "."
            "../external_libs/frozen"
            "../header_only/frozen/include"
            "../header_only/ctre/single-header")

    target_link_libraries(${COMPONENT_LIB} PRIVATE pico_driver_i2c)

    add_custom_target(deletebloat ALL
            COMMAND rm ${CMAKE_CURRENT_SOURCE_DIR}/../gui_smart_aq/build/*.map
            || true)
else()
    add_library(container_utils INTERFACE
        ${PLATFORM_INDEPENDENT_LIB_SOURCES})
    target_include_directories(container_utils INTERFACE .)
endif()


# spiffs_create_partition_image(storage ../gui_smart_aq/build/ FLASH_IN_PROJECT)
# fatfs_create_spiflash_image(values ../values FLASH_IN_PROJECT)