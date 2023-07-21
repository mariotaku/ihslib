target_sources(ihslib-hid-sdl PRIVATE
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

configure_file(config.h.in ${GEN_INCLUDE_DIR}/ihslib/hid/sdl/config.h @ONLY)