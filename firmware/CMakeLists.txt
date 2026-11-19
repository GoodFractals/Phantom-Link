cmake_minimum_required(VERSION 3.13)

include(pico_sdk_import.cmake)

project(phantom_link C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

set(PICO_BOARD pico2_w CACHE STRING "Board type")

pico_sdk_init()

function(add_phantom_target target_name is_ap)
    add_executable(${target_name} main.c)

    target_compile_definitions(${target_name} PRIVATE IS_AP=${is_ap})

    target_compile_options(${target_name} PRIVATE -Wall -Wextra -Werror)

    target_include_directories(${target_name} PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}
    )

    target_link_libraries(${target_name}
        pico_stdlib
        pico_cyw43_arch_lwip_poll
        hardware_spi
        hardware_adc
    )

    pico_enable_stdio_usb(${target_name} 1)
    pico_enable_stdio_uart(${target_name} 0)
    pico_add_extra_outputs(${target_name})
endfunction()

add_phantom_target(phantom_link_ap 1)
add_phantom_target(phantom_link_sta 0)
