add_library(ihs-protobuf STATIC
        pb_utils.c
        discovery.pb-c.c
        remoteplay.pb-c.c)
target_include_directories(ihs-protobuf PRIVATE ../ PUBLIC SYSTEM ${PROTOBUF_C_INCLUDE_DIRS})
target_link_libraries(ihs-protobuf PUBLIC ${PROTOBUF_C_LIBRARIES})

# Don't include tools and samples if used as library
if (IHSLIB_CODEGEN)
    find_program(PROTOC_C protoc-c)
    find_program(PYTHON python3)

    set(PROTOBUF_FILES protobuf/discovery.proto protobuf/remoteplay.proto)
    add_custom_target(protoc-gen
            COMMAND ${PROTOC_C} --c_out=src ${PROTOBUF_FILES}
            COMMAND ${PYTHON} scripts/protobuf-src-fixup.py src/protobuf
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
endif ()