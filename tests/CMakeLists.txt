cmake_policy(SET CMP0135 NEW)

include(FetchContent)
FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/refs/tags/v1.15.2.zip
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

add_executable(container_tests
        ring_buffer_tests.cpp
        sample_container_tests.cpp
        lookup_table_tests.cpp)
target_link_libraries(container_tests PUBLIC container_utils)
target_link_libraries(container_tests PUBLIC GTest::gtest_main)

include(GoogleTest)
gtest_add_tests(TARGET container_tests)