add_library(ihslib-hid-sdl STATIC
        sdl_hid_provider.c
        sdl_hid_enumerator_managed.c
        sdl_hid_enumerator_unmanaged.c
        sdl_hid_enumerator_common.c
        sdl_hid_device.c
        sdl_hid_write.c
        sdl_hid_feature_report.c
        sdl_hid_event.c
        sdl_hid_manager.c
        sdl_hid_report.c
        sdl_hid_utils.c)
target_link_libraries(ihslib-hid-sdl PUBLIC ihslib PRIVATE ihs-protobuf ${SDL2_LIBRARIES})
target_include_directories(ihslib-hid-sdl PUBLIC include PRIVATE ../.. SYSTEM PRIVATE ${SDL2_INCLUDE_DIRS})

if (IHSLIB_SDL_TARGETVERSION)
    string(REGEX MATCH "^([0-9]+)\\.([0-9]+)\\.([0-9]+)$" SDL_SEMVER "${IHSLIB_SDL_TARGETVERSION}")
    if (SDL_SEMVER)
        math(EXPR VERSION_NUM "${CMAKE_MATCH_1} * 1000 + ${CMAKE_MATCH_2} * 100 + ${CMAKE_MATCH_3}")
    else()
        set(VERSION_NUM ${IHSLIB_SDL_TARGETVERSION})
    endif ()
    target_compile_definitions(ihslib-hid-sdl PRIVATE IHS_SDL_TARGETVERSION=${VERSION_NUM})
    set_target_properties(ihslib-hid-sdl PROPERTIES SDL_TARGETVERSION ${VERSION_NUM})
endif ()