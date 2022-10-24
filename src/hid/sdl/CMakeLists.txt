add_library(ihslib-hid-sdl STATIC sdl_hid_provider.c sdl_hid_enumerator.c sdl_hid_device.c)
target_link_libraries(ihslib-hid-sdl PUBLIC ihslib)
target_include_directories(ihslib-hid-sdl PRIVATE ../..)