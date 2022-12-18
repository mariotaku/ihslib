/*
 *  _____  _   _  _____  _  _  _     
 * |_   _|| | | |/  ___|| |(_)| |     Steam    
 *   | |  | |_| |\ `--. | | _ | |__     In-Home
 *   | |  |  _  | `--. \| || || '_ \      Streaming
 *  _| |_ | | | |/\__/ /| || || |_) |       Library
 *  \___/ \_| |_/\____/ |_||_||_.__/
 *
 * Copyright (c) 2022 Ningyuan Li <https://github.com/mariotaku>.
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "ihslib/hid.h"
#include "sdl_hid_common.h"
#include "hid/provider.h"

#include <SDL2/SDL.h>

typedef struct HIDProviderSDL {
    IHS_HIDProvider base;
    bool manageDevice;
} HIDProviderSDL;

static IHS_HIDProvider *ProviderAlloc(const IHS_HIDProviderClass *cls);

static void ProviderFree(IHS_HIDProvider *provider);

static bool ProviderSupportsDevice(IHS_HIDProvider *provider, const char *path);

static IHS_HIDDevice *ProviderOpenDevice(IHS_HIDProvider *provider, const char *path);

static bool ProviderHasChange(IHS_HIDProvider *provider);

static IHS_Enumeration *ProviderEnumerate(IHS_HIDProvider *provider);

static void ProviderDeviceInfo(IHS_HIDProvider *provider, IHS_Enumeration *enumeration,
                               IHS_HIDDeviceInfo *info);

static const IHS_HIDProviderClass ProviderClass = {
        .alloc = ProviderAlloc,
        .free = ProviderFree,
        .supportsDevice = ProviderSupportsDevice,
        .openDevice = ProviderOpenDevice,
        .hasChange = ProviderHasChange,
        .enumerateDevices = ProviderEnumerate,
        .deviceInfo = ProviderDeviceInfo,
};

IHS_Enumeration *IHS_HIDDeviceSDLEnumerate();

bool IHS_HIDDeviceSDLDeviceInfo(IHS_Enumeration *enumeration, IHS_HIDDeviceInfo *info);

IHS_HIDProvider *IHS_HIDProviderSDLCreate(bool manageDevice) {
    HIDProviderSDL *provider = (HIDProviderSDL *) IHS_SessionHIDProviderCreate(&ProviderClass);
    provider->manageDevice = manageDevice;
    SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER | SDL_INIT_HAPTIC);
    return (IHS_HIDProvider *) provider;
}

void IHS_HIDProviderSDLDestroy(IHS_HIDProvider *provider) {
    SDL_QuitSubSystem(SDL_INIT_GAMECONTROLLER | SDL_INIT_HAPTIC);
    IHS_SessionHIDProviderDestroy(provider);
}

static IHS_HIDProvider *ProviderAlloc(const IHS_HIDProviderClass *cls) {
    HIDProviderSDL *provider = calloc(1, sizeof(HIDProviderSDL));
    provider->base.cls = cls;
    return (IHS_HIDProvider *) provider;
}

static void ProviderFree(IHS_HIDProvider *provider) {
    free(provider);
}

static bool ProviderSupportsDevice(IHS_HIDProvider *provider, const char *path) {
    (void) provider;
    return strstr(path, "sdl://") == path;
}

static IHS_HIDDevice *ProviderOpenDevice(IHS_HIDProvider *provider, const char *path) {
    HIDProviderSDL *sdlProvider = (HIDProviderSDL *) provider;
    assert(strstr(path, "sdl://") == path);
    char *endptr = NULL;
    const char *begin = &path[6];
    int deviceId = (int) strtol(begin, &endptr, 10);
    if (endptr == begin) {
        return NULL;
    }
    SDL_GameController *controller = NULL;
#if IHS_SDL_TARGET_ATLEAST(2, 0, 6)
    if (sdlProvider->manageDevice) {
        int index = -1;
        for (int i = 0, numJoysticks = SDL_NumJoysticks(); i < numJoysticks; i++) {
            if (deviceId == SDL_JoystickGetDeviceInstanceID(i)) {
                index = i;
                break;
            }
        }
        if (index >= 0) {
            controller = SDL_GameControllerOpen(index);
        }
    } else {
        controller = SDL_GameControllerFromInstanceID((SDL_JoystickID) deviceId);
    }
#else
    // SDL_GameControllerOpen will return opened controller, so no extra check needed
    controller = SDL_GameControllerOpen(deviceId);
#endif
    if (controller == NULL) {
        return NULL;
    }
    return IHS_HIDDeviceSDLCreate(controller, sdlProvider->manageDevice);
}

static bool ProviderHasChange(IHS_HIDProvider *provider) {
    (void) provider;
    return false;
}

static IHS_Enumeration *ProviderEnumerate(IHS_HIDProvider *provider) {
    (void) provider;
    return IHS_HIDDeviceSDLEnumerate();
}

static void ProviderDeviceInfo(IHS_HIDProvider *provider, IHS_Enumeration *enumeration,
                               IHS_HIDDeviceInfo *info) {
    (void) provider;
    IHS_HIDDeviceSDLDeviceInfo(enumeration, info);
}