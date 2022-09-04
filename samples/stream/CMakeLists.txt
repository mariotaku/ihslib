list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(sample-stream stream.c requestor.c nullaud.c nullvid.c)

target_include_directories(sample-stream PRIVATE ../ SYSTEM PRIVATE)
target_link_libraries(sample-stream PRIVATE ihslib)
