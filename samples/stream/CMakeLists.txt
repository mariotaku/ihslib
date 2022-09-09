list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR})

pkg_check_modules(GSTREAMER gstreamer-plugins-base-1.0 REQUIRED)
add_executable(sample-stream stream.c requestor.c nullaud.c gstvid.c)

target_include_directories(sample-stream PRIVATE ../ SYSTEM PRIVATE ${GSTREAMER_INCLUDE_DIRS})
target_link_libraries(sample-stream PRIVATE ihslib ${GSTREAMER_LIBRARIES} gstapp-1.0)
