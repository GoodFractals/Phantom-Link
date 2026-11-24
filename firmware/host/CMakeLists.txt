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

add_library(phantom_proto STATIC
    "${PROTO_DIR}/proto_frame.c"
    "${PROTO_DIR}/proto_session.c"
    "${PROTO_DIR}/proto_msg.c"
    "${PROTO_DIR}/proto_txq.c"
    "${PROTO_DIR}/proto_crypto.c"
    "${PROTO_DIR}/proto_lpi.c"
    "${PROTO_DIR}/proto_sfo.c"
    "${PROTO_DIR}/proto_sfo_telemetry.c"
    "${PROTO_DIR}/proto_reconnect.c"
    "${PROTO_DIR}/proto_fs.c"
    "${PROTO_DIR}/proto_zeroize.c"
    "${PROTO_DIR}/proto_aead.c"
)
target_include_directories(phantom_proto PUBLIC "${PROTO_DIR}")
target_compile_options(phantom_proto PRIVATE ${WARNINGS})

add_executable(test_frame tests/test_frame.c)
target_link_libraries(test_frame PRIVATE phantom_proto)
target_compile_options(test_frame PRIVATE ${WARNINGS})
add_test(NAME frame COMMAND test_frame)

add_executable(test_session tests/test_session.c)
target_link_libraries(test_session PRIVATE phantom_proto)
target_compile_options(test_session PRIVATE ${WARNINGS})
add_test(NAME session COMMAND test_session)

add_executable(test_wire tests/test_wire.c)
target_link_libraries(test_wire PRIVATE phantom_proto)
target_compile_options(test_wire PRIVATE ${WARNINGS})
add_test(NAME wire COMMAND test_wire)

add_executable(test_handshake tests/test_handshake.c)
target_link_libraries(test_handshake PRIVATE phantom_proto)
target_compile_options(test_handshake PRIVATE ${WARNINGS})
add_test(NAME handshake COMMAND test_handshake)

add_executable(test_msg tests/test_msg.c)
target_link_libraries(test_msg PRIVATE phantom_proto)
target_compile_options(test_msg PRIVATE ${WARNINGS})
add_test(NAME msg COMMAND test_msg)

add_executable(test_reliable tests/test_reliable.c)
target_link_libraries(test_reliable PRIVATE phantom_proto)
target_compile_options(test_reliable PRIVATE ${WARNINGS})
add_test(NAME reliable COMMAND test_reliable)

add_executable(test_txq tests/test_txq.c)
target_link_libraries(test_txq PRIVATE phantom_proto)
target_compile_options(test_txq PRIVATE ${WARNINGS})
add_test(NAME txq COMMAND test_txq)

add_executable(test_crypto tests/test_crypto.c)
target_link_libraries(test_crypto PRIVATE phantom_proto)
target_compile_options(test_crypto PRIVATE ${WARNINGS})
add_test(NAME crypto COMMAND test_crypto)

add_executable(test_sfo tests/test_sfo.c)
target_link_libraries(test_sfo PRIVATE phantom_proto)
target_compile_options(test_sfo PRIVATE ${WARNINGS})
add_test(NAME sfo COMMAND test_sfo)

add_executable(test_sfo_telemetry tests/test_sfo_telemetry.c)
target_link_libraries(test_sfo_telemetry PRIVATE phantom_proto)
target_compile_options(test_sfo_telemetry PRIVATE ${WARNINGS})
add_test(NAME sfo_telemetry COMMAND test_sfo_telemetry)

add_executable(frame_probe "${CMAKE_CURRENT_LIST_DIR}/../../tools/frame_probe.c")
target_link_libraries(frame_probe PRIVATE phantom_proto)
target_compile_options(frame_probe PRIVATE ${WARNINGS})

add_executable(test_banner tests/test_banner.c)
target_include_directories(test_banner PRIVATE "${CMAKE_CURRENT_LIST_DIR}/..")
target_compile_definitions(test_banner PRIVATE PHANTOM_BUILD_HASH="testhash")
target_compile_options(test_banner PRIVATE ${WARNINGS})
add_test(NAME banner COMMAND test_banner)

add_executable(test_reconnect tests/test_reconnect.c)
target_link_libraries(test_reconnect PRIVATE phantom_proto)
target_compile_options(test_reconnect PRIVATE ${WARNINGS})
add_test(NAME reconnect COMMAND test_reconnect)

add_executable(test_fs tests/test_fs.c)
target_link_libraries(test_fs PRIVATE phantom_proto)
target_compile_options(test_fs PRIVATE ${WARNINGS})
add_test(NAME fs COMMAND test_fs)

add_executable(test_zeroize tests/test_zeroize.c)
target_link_libraries(test_zeroize PRIVATE phantom_proto)
target_compile_options(test_zeroize PRIVATE ${WARNINGS})
add_test(NAME zeroize COMMAND test_zeroize)

add_executable(test_aead tests/test_aead.c)
target_link_libraries(test_aead PRIVATE phantom_proto)
target_compile_options(test_aead PRIVATE ${WARNINGS})
add_test(NAME aead COMMAND test_aead)
