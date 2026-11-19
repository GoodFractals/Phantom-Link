cmake_minimum_required(VERSION 3.20)

project(phantom_link_host C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

enable_testing()

set(PROTO_DIR "${CMAKE_CURRENT_LIST_DIR}/../proto")
if(MSVC)
    set(WARNINGS /W4 /WX)
else()
    set(WARNINGS -Wall -Wextra -Werror)
endif()

add_executable(test_spec tests/test_spec.c)
target_include_directories(test_spec PRIVATE "${PROTO_DIR}")
target_compile_options(test_spec PRIVATE ${WARNINGS})
add_test(NAME spec COMMAND test_spec)
