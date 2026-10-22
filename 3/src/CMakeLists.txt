cmake_minimum_required(VERSION 3.16)
project(lab3_variant9 C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

find_package(Threads REQUIRED)
add_executable(determinant determinant.c)
target_link_libraries(determinant PRIVATE Threads::Threads)
target_compile_options(determinant PRIVATE -Wall -Wextra -Wpedantic)

enable_testing()
add_test(NAME basic COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_basic.sh $<TARGET_FILE:determinant>)
find_package(Python3 COMPONENTS Interpreter QUIET)
if(Python3_Interpreter_FOUND)
    add_test(NAME exact_oracle COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_determinant.py $<TARGET_FILE:determinant>)
    set_tests_properties(exact_oracle PROPERTIES TIMEOUT 120)
endif()
