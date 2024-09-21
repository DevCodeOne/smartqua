# The following lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.16)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-missing-field-initializers")

if (NOT "$ENV{IDF_PATH}" STREQUAL "")
    message(WARNING "Building as esp32 project")
    set(EXTRA_COMPONENT_DIRS
            ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/esp-idf-lib/components
            ${CMAKE_CURRENT_SOURCE_DIR}/smartaq
    )

    include($ENV{IDF_PATH}/tools/cmake/project.cmake)
    project(smartaq)

    set(BUILD_PICO_DEVICE_DRIVER OFF)
    add_subdirectory(external_libs/pico-driver)
else()
    option(SMARTAQ_BUILD_TESTS "Enable tests" ON)
    project(smartaq)
    add_subdirectory(smartaq)

    if (BUILD_TESTS)
        enable_testing()

        add_subdirectory(tests)
    endif()
endif()
