#include "config.h"

#if IHSLIB_SDL2_HIDAPI
#include <SDL.h>

#include "ihslib/hidapi.h"

const static IHS_StreamHIDInterface HIDAPIInterface = {
        .init = (void *) SDL_hid_init,
        .exit = (void *) SDL_hid_exit,
        .enumerate = (void *) SDL_hid_enumerate,
        .free_enumeration = (void *) SDL_hid_free_enumeration,
        .open = (void *) SDL_hid_open,
        .open_path = (void *) SDL_hid_open_path,
        .write = (void *) SDL_hid_write,
        .read_timeout = (void *) SDL_hid_read_timeout,
        .read = (void *) SDL_hid_read,
        .set_nonblocking = (void *) SDL_hid_set_nonblocking,
        .send_feature_report = (void *) SDL_hid_send_feature_report,
        .get_feature_report = (void *) SDL_hid_get_feature_report,
        .close = (void *) SDL_hid_close,
};

const IHS_StreamHIDInterface *IHS_StreamHIDInterfaceSDL() {
    return &HIDAPIInterface;
}

#endif