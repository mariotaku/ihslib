add_executable(sample-stream stream.c requestor.c pipeline.c)

pkg_check_modules(GSTREAMER gstreamer-1.0 gstreamer-app-1.0 REQUIRED)

target_include_directories(sample-stream PRIVATE ${CMAKE_SOURCE_DIR}/samples SYSTEM PRIVATE ${GSTREAMER_INCLUDE_DIRS})
target_link_libraries(sample-stream PRIVATE ihslib ${GSTREAMER_LIBRARIES})
