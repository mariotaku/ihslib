cmake_minimum_required(VERSION 3.16)
project(ihslib C)

set(CMAKE_C_STANDARD 11)
list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/scripts)

find_program(PROTOC_C protoc-c)
find_program(PYTHON python3)
find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBUV libuv REQUIRED)
pkg_check_modules(PROTOBUF libprotobuf-c REQUIRED)

find_package(MbedTLS)
find_package(OpenSSL)

set(PROTOBUF_FILES protobuf/discovery.proto protobuf/remoteplay.proto)

file(GLOB PROTOBUF_SOURCES src/protobuf/*.c)
add_library(ihs-protobuf STATIC ${PROTOBUF_SOURCES} include/ihslib/crypto.h)
target_include_directories(ihs-protobuf PRIVATE src PRIVATE SYSTEM ${PROTOBUF_INCLUDE_DIRS})
target_link_libraries(ihs-protobuf PUBLIC ${PROTOBUF_LIBRARIES})

add_library(ihslib
        src/endianness.c
        src/client/client.c
        src/client/discovery.c
        src/client/authorization.c
        src/client/streaming.c)

target_include_directories(ihslib PUBLIC include PRIVATE src PRIVATE SYSTEM ${LIBUV_INCLUDE_DIRS})
target_link_libraries(ihslib PUBLIC ${LIBUV_LIBRARIES} ihs-protobuf)

if (MBEDTLS_FOUND)
    target_sources(ihslib PRIVATE src/crypto/impl_mbedtls.c)
    target_include_directories(ihslib PRIVATE SYSTEM ${MBEDTLS_INCLUDE_DIRS})
    target_link_libraries(ihslib PUBLIC ${MBEDCRYPTO_LIBRARY})
elseif(OPENSSL_FOUND)
    target_sources(ihslib PRIVATE src/crypto/impl_openssl.c)
endif()

add_custom_target(protoc-gen
        COMMAND ${PROTOC_C} --c_out=src ${PROTOBUF_FILES}
        COMMAND ${PYTHON} scripts/protobuf-src-fixup.py src/protobuf
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

add_subdirectory(samples)