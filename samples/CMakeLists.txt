add_executable(sample-discover discover.c common.h)
target_link_libraries(sample-discover ihslib)

add_executable(sample-authorize authorize.c common.h)
target_link_libraries(sample-authorize ihslib)

add_executable(sample-stream stream.c common.h)
target_link_libraries(sample-stream ihslib)