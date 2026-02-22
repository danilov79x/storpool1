cmake_minimum_required(VERSION 3.16)

project(unique_count C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

add_executable(unique_count unique_count.c)
target_include_directories(unique_count PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_compile_options(unique_count PRIVATE
    -Wall
    -Wextra
    -Wpedantic
)

add_executable(generate_uint32 generate_uint32.c)
target_compile_options(generate_uint32 PRIVATE
    -Wall
    -Wextra
    -Wpedantic
)
target_link_libraries(generate_uint32 PRIVATE m)

enable_testing()

add_executable(unique_count_tests
    tests/test_unique_count.c
    unique_count.c
)
target_include_directories(unique_count_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(unique_count_tests PRIVATE UNIQUE_COUNT_NO_MAIN)
target_compile_options(unique_count_tests PRIVATE
    -Wall
    -Wextra
    -Wpedantic
)
add_test(NAME unique_count_tests COMMAND unique_count_tests)
