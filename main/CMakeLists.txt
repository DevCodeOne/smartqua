idf_component_register(SRCS "bottle_scale_main.cpp"
                            "../components/frozen/frozen.c"
                    INCLUDE_DIRS "../components/frozen")

add_custom_target(deletebloat ALL
    COMMAND true || rm ${CMAKE_CURRENT_SOURCE_DIR}/../co2_bottle_scale/build/*.map)

spiffs_create_partition_image(storage ../co2_bottle_scale/build/ FLASH_IN_PROJECT)
