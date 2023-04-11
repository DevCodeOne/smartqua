idf_component_register(SRCS     "aq_main.cpp"
                                "actions/device_actions.cpp"
                                "actions/stats_actions.cpp"
                                "rest/scale_rest.cpp"
                                "rest/devices_rest.cpp"
                                "rest/stats_rest.cpp"
                                "drivers/device_resource.cpp"
                                "drivers/devices.cpp"
                                "drivers/pwm.cpp"
                                "drivers/dac_driver.cpp"
                                "drivers/ds18x20_driver.cpp"
                                "drivers/ads111x_driver.cpp"
                                "drivers/scale_driver.cpp"
                                "drivers/schedule_driver.cpp"
                                "drivers/q30_driver.cpp"
                                "drivers/stepper_dosing_pump_driver.cpp"
                                "drivers/rmt_stepper_driver.cpp"
                                "utils/idf_utils.cpp"
                                # "utils/sd_filesystem.cpp"
                                "utils/filesystem_utils.cpp"
                                "utils/time_utils.cpp"
                                "utils/logger.cpp"
                                "utils/web_utils.cpp"
                            "../components/frozen/frozen.c"
                    INCLUDE_DIRS 
                    "."
                    "../components/frozen"
                    "../header_only/ctre/single-header")

add_custom_target(deletebloat ALL
    COMMAND rm ${CMAKE_CURRENT_SOURCE_DIR}/../gui_smart_aq/build/*.map 
            || true)

# spiffs_create_partition_image(storage ../gui_smart_aq/build/ FLASH_IN_PROJECT)
