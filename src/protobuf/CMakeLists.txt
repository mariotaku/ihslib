add_library(ihs-protobuf STATIC
        pb_utils.c
        discovery.pb-c.c
        remoteplay.pb-c.c
        hiddevices.pb-c.c)
target_include_directories(ihs-protobuf PRIVATE ../ ../../include PUBLIC SYSTEM ${PROTOBUF_C_INCLUDEDIR})
target_link_libraries(ihs-protobuf PUBLIC ${PROTOBUF_C_LIBRARIES})