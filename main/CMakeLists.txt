idf_component_register(SRCS     "aq_main.cpp"
                                "scale.cpp"
                                "scale_rest.cpp"
                                "devices.cpp"
                                "devices.h"
                                "pwm.cpp"
                                "pwm_rest.cpp"
                            "../components/frozen/frozen.c"
                    INCLUDE_DIRS "../components/frozen")

add_custom_target(deletebloat ALL
    COMMAND rm ${CMAKE_CURRENT_SOURCE_DIR}/../gui_smart_aq/build/*.map 
            || true)

spiffs_create_partition_image(storage ../gui_smart_aq/build/ FLASH_IN_PROJECT)
