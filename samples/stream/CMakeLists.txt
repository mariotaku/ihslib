list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(sample-stream stream.c requestor.c nullaud.c nullvid.c sps_parser.c)

find_package(Threads REQUIRED)

target_include_directories(sample-stream PRIVATE ../ SYSTEM PRIVATE ${GSTREAMER_INCLUDE_DIRS}
        ${BROADCOM_INCLUDE_DIRS} ${OPUS_INCLUDE_DIRS} ${ALSA_INCLUDE_DIRS})
target_link_libraries(sample-stream PRIVATE Threads::Threads ihslib ${GSTREAMER_LIBRARIES} ${BROADCOM_MMAL_LIBRARIES}
        ${OPUS_LIBRARIES} ${ALSA_LIBRARIES})
