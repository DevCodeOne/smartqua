idf_component_register(SRCS     "aq_main.cpp"
                                "drivers/scale.cpp"
                                "utils/http_utils.cpp"
                                "rest/scale_rest.cpp"
                                "rest/pwm_rest.cpp"
                                "rest/devices_rest.cpp"
                                "drivers/devices.cpp"
                                "drivers/devices.h"
                                "drivers/pwm.cpp"
                                "drivers/ds18x20_driver.cpp"
                            "../components/frozen/frozen.c"
                    INCLUDE_DIRS 
                    "."
                    "../components/frozen")

add_custom_target(deletebloat ALL
    COMMAND rm ${CMAKE_CURRENT_SOURCE_DIR}/../gui_smart_aq/build/*.map 
            || true)

spiffs_create_partition_image(storage ../gui_smart_aq/build/ FLASH_IN_PROJECT)
