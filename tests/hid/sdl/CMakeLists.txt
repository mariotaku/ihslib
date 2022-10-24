ihs_add_test(hid_sdl test_sdl_hid.c)
ihs_test_link_libraries(hid_sdl ihslib-hid-sdl ${SDL2_LIBRARIES})