ihs_add_test(hid_sdl_device_managed test_sdl_hid_device_managed.c)
target_link_libraries("${IHSTEST_TARGET}" PRIVATE ihslib-hid-sdl ihs-test-session ${SDL2_LIBRARIES})
get_target_property(SDL_TARGETVERSION ihslib-hid-sdl SDL_TARGETVERSION)
if (SDL_TARGETVERSION)
    target_compile_definitions("${IHSTEST_TARGET}" PRIVATE SDL_TARGETVERSION=${SDL_TARGETVERSION})
endif()

ihs_add_test(hid_sdl_device_unmanaged test_sdl_hid_device_unmanaged.c)
target_link_libraries("${IHSTEST_TARGET}" PRIVATE ihslib-hid-sdl ihs-test-session ${SDL2_LIBRARIES})

ihs_add_test(hid_sdl_report test_sdl_hid_report.c)
target_link_libraries("${IHSTEST_TARGET}" PRIVATE ihslib-hid-sdl ihs-test-session ${SDL2_LIBRARIES})