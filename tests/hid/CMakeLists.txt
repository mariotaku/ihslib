ihs_add_test(report_data report_data.c)

ihs_add_test(hid_manager hid_manager.c)
target_link_libraries("${IHSTEST_TARGET}" PRIVATE ihs-test-session)

add_subdirectory(sdl)