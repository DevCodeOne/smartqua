idf_component_register(SRCS     "aq_main.cpp"
                                "actions/device_actions.cpp"
                                "actions/soft_timer_actions.cpp"
                                "drivers/scale.cpp"
                                "drivers/soft_timers.cpp"
                                "utils/http_utils.cpp"
                                "rest/scale_rest.cpp"
                                "rest/devices_rest.cpp"
                                "rest/soft_timers_rest.cpp"
                                "drivers/devices.cpp"
                                "drivers/devices.h"
                                "drivers/pwm.cpp"
                                "drivers/ds18x20_driver.cpp"
                                "utils/idf-utils.cpp"
                            "../components/frozen/frozen.c"
                    INCLUDE_DIRS 
                    "."
                    "../components/frozen")

add_custom_target(deletebloat ALL
    COMMAND rm ${CMAKE_CURRENT_SOURCE_DIR}/../gui_smart_aq/build/*.map 
            || true)

spiffs_create_partition_image(storage ../gui_smart_aq/build/ FLASH_IN_PROJECT)
