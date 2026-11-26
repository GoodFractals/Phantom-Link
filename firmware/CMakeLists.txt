cmake_minimum_required(VERSION 3.13)

include(pico_sdk_import.cmake)

project(phantom_link C CXX ASM)
set(PICO_MBEDTLS_CONFIG_FILE "phantom_mbedtls_config.h" CACHE STRING "Phantom Link mbedTLS config")
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

set(PICO_BOARD pico2_w CACHE STRING "Board type")

# Build identity: the first eight hex digits of main.c's SHA256. The directory
# dependency makes CMake re-run when main.c changes, so the banner cannot go
# stale; rebuilding the same tree produces the same hash.
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_LIST_DIR}/main.c")
file(SHA256 "${CMAKE_CURRENT_LIST_DIR}/main.c" PHANTOM_SOURCE_HASH)
string(SUBSTRING "${PHANTOM_SOURCE_HASH}" 0 8 PHANTOM_BUILD_HASH)

pico_sdk_init()

function(add_phantom_target target_name is_ap)
    add_executable(${target_name} main.c proto/proto_frame.c proto/proto_session.c proto/proto_msg.c proto/proto_txq.c proto/proto_crypto.c proto/proto_lpi.c proto/proto_sfo.c proto/proto_sfo_telemetry.c proto/proto_reconnect.c proto/proto_zeroize.c proto/proto_aead.c proto/proto_fs.c proto/proto_x25519.c)

    target_compile_definitions(${target_name} PRIVATE
        IS_AP=${is_ap}
        PHANTOM_BUILD_HASH="${PHANTOM_BUILD_HASH}"
    )

    target_compile_options(${target_name} PRIVATE -Wall -Wextra -Werror)

    target_include_directories(${target_name} PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}
        ${CMAKE_CURRENT_LIST_DIR}/proto
        ${PICO_SDK_PATH}/lib/mbedtls/include
        ${PICO_SDK_PATH}/lib/mbedtls/include/mbedtls
        ${CMAKE_CURRENT_LIST_DIR}/proto
    )

    target_link_libraries(${target_name}
        pico_stdlib
        pico_rand
        pico_cyw43_arch_lwip_poll
        pico_mbedtls_crypto
        pico_mbedtls_headers
        hardware_spi
        hardware_adc
    )

    pico_enable_stdio_usb(${target_name} 1)
    pico_enable_stdio_uart(${target_name} 0)
    pico_add_extra_outputs(${target_name})
endfunction()

add_phantom_target(phantom_link_ap 1)
add_phantom_target(phantom_link_sta 0)

option(PHANTOM_WATCHDOG_TEST "Build forced-hang watchdog test firmware" OFF)
option(PHANTOM_USE_AEAD "Use ChaCha20-Poly1305 instead of Speck CTR+CMAC" OFF)
option(PHANTOM_STATUS_SAVE "Enable STATUS flash writes (ticket 20; verified fixed)" ON)
option(PHANTOM_BRINGUP_TRACE "Draw bring-up checkpoints on the OLED" OFF)
if(PHANTOM_USE_AEAD)
    target_compile_definitions(phantom_link_ap PRIVATE PHANTOM_USE_AEAD=1)
    target_compile_definitions(phantom_link_sta PRIVATE PHANTOM_USE_AEAD=1)
endif()
if(PHANTOM_STATUS_SAVE)
    target_compile_definitions(phantom_link_ap PRIVATE PHANTOM_STATUS_SAVE=1)
    target_compile_definitions(phantom_link_sta PRIVATE PHANTOM_STATUS_SAVE=1)
endif()
if(PHANTOM_BRINGUP_TRACE)
    target_compile_definitions(phantom_link_ap PRIVATE PHANTOM_BRINGUP_TRACE=1)
    target_compile_definitions(phantom_link_sta PRIVATE PHANTOM_BRINGUP_TRACE=1)
endif()
if(PHANTOM_WATCHDOG_TEST)
    target_compile_definitions(phantom_link_ap PRIVATE PHANTOM_WATCHDOG_TEST=1)
    target_compile_definitions(phantom_link_sta PRIVATE PHANTOM_WATCHDOG_TEST=1)
endif()
