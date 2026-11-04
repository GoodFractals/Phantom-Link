cmake_minimum_required(VERSION 3.13)

include(pico_sdk_import.cmake)

project(phantom_link_rx C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

set(PICO_BOARD pico2_w CACHE STRING "Board type")

pico_sdk_init()

add_executable(phantom_link_rx
    main.c
)

target_include_directories(phantom_link_rx PRIVATE 
    ${CMAKE_CURRENT_LIST_DIR}
)

target_link_libraries(phantom_link_rx 
    pico_stdlib 
    pico_cyw43_arch_lwip_poll 
    hardware_spi 
    hardware_adc
)

pico_enable_stdio_usb(phantom_link_rx 1)
pico_enable_stdio_uart(phantom_link_rx 0)

pico_add_extra_outputs(phantom_link_rx)