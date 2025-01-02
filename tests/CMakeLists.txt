cmake_policy(SET CMP0135 NEW)

include(FetchContent)
FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/refs/tags/v1.15.2.zip
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

add_executable(smartaq_tests
        basic_stack_string_tests.cpp
        check_assign_tests.cpp
        ring_buffer_tests.cpp
        sample_container_tests.cpp
        lookup_table_tests.cpp
        day_schedule_tests.cpp
        schedule_tests.cpp
        schedule_tracker_tests.cpp
        fixed_size_optional_array_tests.cpp
        time_utils_tests.cpp)
target_link_libraries(smartaq_tests PUBLIC smartaq_lib)
target_link_libraries(smartaq_tests PUBLIC GTest::gtest_main)
target_compile_options(smartaq_tests PRIVATE -coverage)
target_link_options(smartaq_tests PRIVATE -coverage)

include(GoogleTest)
gtest_add_tests(TARGET smartaq_tests WORKING_DIRECTORY ${CMAKE_BINARY_DIR})