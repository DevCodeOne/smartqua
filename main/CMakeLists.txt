idf_component_register(SRCS     "aq_main.cpp"
                                "actions/device_actions.cpp"
                                "actions/soft_timer_actions.cpp"
                                "actions/stats_actions.cpp"
                                "drivers/scale.cpp"
                                "drivers/lamp_driver.cpp"
                                "rest/scale_rest.cpp"
                                "rest/devices_rest.cpp"
                                "rest/soft_timers_rest.cpp"
                                "rest/stats_rest.cpp"
                                "drivers/device_resource.cpp"
                                "drivers/devices.cpp"
                                "drivers/pwm.cpp"
                                "drivers/dac_driver.cpp"
                                "drivers/ds18x20_driver.cpp"
                                "utils/idf-utils.cpp"
                                "utils/sd_filesystem.cpp"
                                "utils/filesystem_utils.cpp"
                                "utils/time_utils.cpp"
                                "utils/sd_card_logger.cpp"
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
