add_library(ihslib-hid-sdl STATIC
        sdl_hid_provider.c
        sdl_hid_enumerator.c
        sdl_hid_device.c
        sdl_hid_write.c
        sdl_hid_feature_report.c
        sdl_hid_event.c
        )
target_link_libraries(ihslib-hid-sdl PUBLIC ihslib)
target_include_directories(ihslib-hid-sdl PUBLIC include PRIVATE ../..)