ihs_add_test(hid_sdl_device test_sdl_hid_device.c)
target_link_libraries("${IHSTEST_TARGET}" PRIVATE ihslib-hid-sdl ihs-test-session ${SDL2_LIBRARIES})

ihs_add_test(hid_sdl_report test_sdl_hid_report.c)
target_link_libraries("${IHSTEST_TARGET}" PRIVATE ihslib-hid-sdl ihs-test-session ${SDL2_LIBRARIES})