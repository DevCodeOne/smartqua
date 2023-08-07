idf_component_register(SRCS     "aq_main.cpp"
                                "actions/device_actions.cpp"
                                "actions/stats_actions.cpp"
                                "rest/devices_rest.cpp"
                                "rest/stats_rest.cpp"
                                "drivers/device_resource.cpp"
                                "drivers/devices.cpp"
                                "drivers/pin_driver.cpp"
                                "drivers/dac_driver.cpp"
                                "drivers/ds18x20_driver.cpp"
                                "drivers/ads111x_driver.cpp"
                                "drivers/pcf8575_driver.cpp"
                                "drivers/scale_driver.cpp"
                                "drivers/schedule_driver.cpp"
                                # "drivers/q30_driver.cpp"
                                "drivers/stepper_dosing_pump_driver.cpp"
                                "drivers/drv8825_driver.cpp"
                                "drivers/ph_probe_driver.cpp"
                                "drivers/switch_driver.cpp"
                                "drivers/rmt_stepper_driver.cpp"
                                "drivers/pico_device_driver.cpp"
                                "utils/idf_utils.cpp"
                                # "storage/sd_filesystem.cpp"
                                "utils/filesystem_utils.cpp"
                                "utils/time_utils.cpp"
                                "utils/logger.cpp"
                                "utils/web_utils.cpp"
                            "../external_libs/frozen/frozen.c"
                    INCLUDE_DIRS 
                    "."
                    "../external_libs/frozen"
                    "../header_only/frozen/include"
                    "../header_only/ctre/single-header")

target_link_libraries(${COMPONENT_LIB} PRIVATE pico_driver_i2c)

add_custom_target(deletebloat ALL
    COMMAND rm ${CMAKE_CURRENT_SOURCE_DIR}/../gui_smart_aq/build/*.map 
            || true)

# spiffs_create_partition_image(storage ../gui_smart_aq/build/ FLASH_IN_PROJECT)
# fatfs_create_spiflash_image(values ../values FLASH_IN_PROJECT)