cmake_minimum_required(VERSION 3.16)
project(lab2_variant12 C)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

add_library(common STATIC common.c)
target_include_directories(common PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_options(common PRIVATE -Wall -Wextra -Wpedantic)
foreach(program parent child1 child2)
    add_executable(${program} ${program}.c)
    target_link_libraries(${program} PRIVATE common)
    target_compile_options(${program} PRIVATE -Wall -Wextra -Wpedantic)
endforeach()

enable_testing()
add_test(NAME pipeline_tests
    COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_pipeline.sh
            $<TARGET_FILE:parent> $<TARGET_FILE:child1> $<TARGET_FILE:child2>)
set_tests_properties(pipeline_tests PROPERTIES TIMEOUT 30)
