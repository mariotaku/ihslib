cmake_minimum_required(VERSION 3.0)

project(ihslib C)

set(CMAKE_C_STANDARD 11)
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/scripts)

find_package(PkgConfig REQUIRED)

pkg_check_modules(SDL2 sdl2)
pkg_check_modules(SDL2_NET SDL2_net)

if (NOT PROTOBUF_C_FOUND)
    pkg_check_modules(PROTOBUF_C libprotobuf-c REQUIRED)
endif ()

if (NOT MBEDTLS_FOUND)
    find_package(MbedTLS)
endif()

if (NOT MBEDTLS_FOUND)
    find_package(OpenSSL)
endif()

set(PROTOBUF_FILES protobuf/discovery.proto protobuf/remoteplay.proto)

file(GLOB PROTOBUF_SOURCES src/protobuf/*.c)
add_library(ihs-protobuf STATIC ${PROTOBUF_SOURCES})
target_include_directories(ihs-protobuf PRIVATE src PUBLIC SYSTEM ${PROTOBUF_C_INCLUDE_DIRS})
target_link_libraries(ihs-protobuf PUBLIC ${PROTOBUF_C_LIBRARIES})

add_library(ihslib
        src/base.c
        src/crc32c.c
        src/memmem.c
        src/ihs_timer.c
        src/ihs_udp.c
        src/ihs_thread.c
        src/ihs_queue.c
        src/client/client.c
        src/client/discovery.c
        src/client/authorization.c
        src/client/streaming.c
        src/session/session.c
        src/session/packet.c
        src/session/frame_window.c
        src/session/frame_crypto.c
        src/session/channels/channel.c
        src/session/channels/ch_discovery.c
        src/session/channels/ch_control.c
        src/session/channels/ch_control_authentication.c
        src/session/channels/ch_control_negotiation.c
        src/session/channels/ch_control_keepalive.c
        src/session/channels/ch_control_audio.c
        src/session/channels/ch_control_video.c
        src/session/channels/ch_data.c
        src/session/channels/ch_data_audio.c
        src/session/channels/video/callback_h264.c
        src/session/channels/video/ch_data_video.c
        src/session/channels/control/control_cursor.c
        src/session/channels/ch_stats.c
        )

target_include_directories(ihslib PUBLIC include PRIVATE src)
target_link_libraries(ihslib PRIVATE ihs-protobuf)

if (SDL2_FOUND)
    target_sources(ihslib PRIVATE src/ihs_thread_sdl.c)
    target_include_directories(ihslib PRIVATE SYSTEM ${SDL2_INCLUDE_DIRS})
    target_link_libraries(ihslib PUBLIC ${SDL2_LIBRARIES})
endif ()

if (SDL2_NET_FOUND)
    target_sources(ihslib PRIVATE src/ihs_udp_sdl.c)
    target_include_directories(ihslib PRIVATE SYSTEM ${SDL2_NET_INCLUDE_DIRS})
    target_link_libraries(ihslib PUBLIC ${SDL2_NET_LIBRARIES})
endif ()

if (MBEDTLS_FOUND)
    target_sources(ihslib PRIVATE src/crypto/impl_mbedtls.c)
    target_include_directories(ihslib PRIVATE SYSTEM ${MBEDTLS_INCLUDE_DIRS})
    target_link_libraries(ihslib PUBLIC ${MBEDCRYPTO_LIBRARY})
elseif (OPENSSL_FOUND)
    target_sources(ihslib PRIVATE src/crypto/impl_openssl.c)
endif ()

# Don't include tools and samples if used as library
if (CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    find_program(PROTOC_C protoc-c)
    find_program(PYTHON python3)

    add_custom_target(protoc-gen
            COMMAND ${PROTOC_C} --c_out=src ${PROTOBUF_FILES}
            COMMAND ${PYTHON} scripts/protobuf-src-fixup.py src/protobuf
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

    add_subdirectory(samples)
endif ()