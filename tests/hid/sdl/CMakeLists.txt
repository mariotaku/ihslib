ihs_add_test(hid_sdl_device test_sdl_hid_device.c)
ihs_test_link_libraries(hid_sdl_device ihslib-hid-sdl ${SDL2_LIBRARIES})

ihs_add_test(hid_sdl_report test_sdl_hid_report.c)
ihs_test_link_libraries(hid_sdl_report ihslib-hid-sdl ${SDL2_LIBRARIES})