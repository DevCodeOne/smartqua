idf_component_register(SRCS "bottle_scale_main.cpp"
                            "../components/frozen/frozen.c"
                    INCLUDE_DIRS "../components/frozen")

spiffs_create_partition_image(storage webapp FLASH_IN_PROJECT)
