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

add_library(phantom_proto STATIC "${PROTO_DIR}/proto_frame.c")
target_include_directories(phantom_proto PUBLIC "${PROTO_DIR}")
target_compile_options(phantom_proto PRIVATE ${WARNINGS})

add_executable(test_frame tests/test_frame.c)
target_link_libraries(test_frame PRIVATE phantom_proto)
target_compile_options(test_frame PRIVATE ${WARNINGS})
add_test(NAME frame COMMAND test_frame)

add_executable(test_banner tests/test_banner.c)
target_include_directories(test_banner PRIVATE "${CMAKE_CURRENT_LIST_DIR}/..")
target_compile_definitions(test_banner PRIVATE PHANTOM_BUILD_HASH="testhash")
target_compile_options(test_banner PRIVATE ${WARNINGS})
add_test(NAME banner COMMAND test_banner)
