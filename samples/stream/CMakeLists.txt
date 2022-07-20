list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(sample-stream stream.c requestor.c alsaaud.c nullvid.c sps_parser.c)

find_package(BroadcomMMAL REQUIRED)
find_package(Threads REQUIRED)
pkg_check_modules(OPUS opus REQUIRED)
pkg_check_modules(ALSA alsa REQUIRED)

target_include_directories(sample-stream PRIVATE ${CMAKE_SOURCE_DIR}/samples SYSTEM PRIVATE ${GSTREAMER_INCLUDE_DIRS}
        ${BROADCOM_INCLUDE_DIRS} ${OPUS_INCLUDE_DIRS} ${ALSA_INCLUDE_DIRS})
target_link_libraries(sample-stream PRIVATE Threads::Threads ihslib ${GSTREAMER_LIBRARIES} ${BROADCOM_MMAL_LIBRARIES}
        ${OPUS_LIBRARIES} ${ALSA_LIBRARIES})
