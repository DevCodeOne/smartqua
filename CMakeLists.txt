# The following lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.11)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-missing-field-initializers")

project(SmartAq C CXX)

option(BUILD_TESTS "Enable tests" ON)

if (NOT "$ENV{IDF_PATH}" STREQUAL "")
    set(EXTRA_COMPONENT_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/components/esp-idf-lib/components)

    include($ENV{IDF_PATH}/tools/cmake/project.cmake)

    set(BUILD_PICO_DEVICE_DRIVER OFF)
    add_subdirectory(external_libs/pico-driver)
else()
    add_subdirectory(main)

    if (BUILD_TESTS)
        enable_testing()

        add_subdirectory(tests)
    endif()
endif()
