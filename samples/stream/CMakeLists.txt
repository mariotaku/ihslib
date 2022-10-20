list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(sample-stream stream.c requestor.c nullaud.c)

target_include_directories(sample-stream PRIVATE ../)
target_link_libraries(sample-stream PRIVATE ihslib)

if (IHSLIB_SAMPLE_STREAM_GST)
    pkg_check_modules(GSTREAMER gstreamer-plugins-base-1.0 REQUIRED)
    target_sources(sample-stream PRIVATE gstvid.c)
    target_include_directories(sample-stream SYSTEM PRIVATE ${GSTREAMER_INCLUDE_DIRS})
    target_link_libraries(sample-stream PRIVATE ${GSTREAMER_LIBRARIES} ${HIDAPI_LIBRARIES} gstapp-1.0)
else ()
    target_sources(sample-stream PRIVATE nullvid.c)
endif ()
