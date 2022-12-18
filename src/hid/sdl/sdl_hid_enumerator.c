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
#include <stddef.h>
#include <stdlib.h>

#include "sdl_hid_common.h"
#include "sdl_hid_utils.h"

#include "ihslib/enumeration.h"
#include "ihslib/hid.h"

typedef struct GameControllerEnumeration {
    IHS_Enumeration base;
    int joystickIndex;
    int joystickCount;

    struct {
        char path[16];
        char name[64];
    } temp;
} GameControllerEnumeration;

static IHS_Enumeration *EnumerationAlloc(const IHS_EnumerationClass *cls);

static size_t EnumerationCount(const IHS_Enumeration *enumeration);

static void EnumerationReset(IHS_Enumeration *enumeration);

static bool EnumerationEnded(const IHS_Enumeration *enumeration);

static void *EnumerationGet(const IHS_Enumeration *enumeration);

static void *EnumerationNext(IHS_Enumeration *enumeration);

static int NextControllerIndex(int current, int count);

const static IHS_EnumerationClass GameControllerEnumerationClass = {
        .alloc = EnumerationAlloc,
        .free = (void (*)(IHS_Enumeration *)) free,
        .count = EnumerationCount,
        .reset = EnumerationReset,
        .ended = EnumerationEnded,
        .get = EnumerationGet,
        .next = EnumerationNext,
};

IHS_Enumeration *IHS_HIDDeviceSDLEnumerate() {
    return IHS_EnumerationCreate(&GameControllerEnumerationClass);
}

bool IHS_HIDDeviceSDLDeviceInfo(IHS_Enumeration *enumeration, IHS_HIDDeviceInfo *info) {
    GameControllerEnumeration *gce = (GameControllerEnumeration *) enumeration;
    int index = gce->joystickIndex;
#if IHS_SDL_TARGET_ATLEAST(2, 0, 6)
    SDL_JoystickID instanceId = SDL_JoystickGetDeviceInstanceID(index);
    if (instanceId == -1) {
        return false;
    }
    snprintf(gce->temp.path, 16, "sdl://%d", instanceId);
    info->vendor_id = SDL_JoystickGetDeviceVendor(index);
    info->product_id = SDL_JoystickGetDeviceProduct(index);
    info->product_version = SDL_JoystickGetDeviceProductVersion(index);
#else
    snprintf(gce->temp.path, 16, "sdl://%d", index);
    SDL_JoystickGUID guid = SDL_JoystickGetDeviceGUID(index);
    const char *name = SDL_JoystickNameForIndex(index);
    if (name != NULL) {
        strncpy(gce->temp.name, name, 63);
        gce->temp.name[63] = '\0';
    } else {
        strcpy(gce->temp.name, "Generic Gamepad");
    }
    if (!IHS_HIDDeviceSDLGetJoystickGUIDInfo(&guid, &info->vendor_id, &info->product_id, &info->product_version,
                                             NULL)) {
        info->vendor_id = 0;
        info->product_id = 0;
        info->product_version = 0;
    }
#endif
    info->path = gce->temp.path;
    info->name = gce->temp.name;
    return true;
}

static IHS_Enumeration *EnumerationAlloc(const IHS_EnumerationClass *cls) {
    GameControllerEnumeration *enumeration = calloc(1, sizeof(GameControllerEnumeration));
    enumeration->base.cls = cls;
    EnumerationReset((IHS_Enumeration *) enumeration);
    return (IHS_Enumeration *) enumeration;
}

static size_t EnumerationCount(const IHS_Enumeration *enumeration) {
    (void) enumeration;
    size_t count = 0;
    const GameControllerEnumeration *gce = (const GameControllerEnumeration *) enumeration;
    for (int i = 0, j = gce->joystickCount; i < j; i++) {
        if (SDL_IsGameController(i)) {
            count++;
        }
    }
    return count;
}

static void EnumerationReset(IHS_Enumeration *enumeration) {
    GameControllerEnumeration *gce = (GameControllerEnumeration *) enumeration;
    gce->joystickIndex = 0;
    gce->joystickCount = SDL_NumJoysticks();
}

static bool EnumerationEnded(const IHS_Enumeration *enumeration) {
    const GameControllerEnumeration *gce = (const GameControllerEnumeration *) enumeration;
    return gce->joystickIndex >= gce->joystickCount;
}

static void *EnumerationGet(const IHS_Enumeration *enumeration) {
    const GameControllerEnumeration *gce = (const GameControllerEnumeration *) enumeration;
    if (gce->joystickIndex < gce->joystickCount) {
        return (void *) &gce->joystickIndex;
    }
    return NULL;
}

static void *EnumerationNext(IHS_Enumeration *enumeration) {
    GameControllerEnumeration *gce = (GameControllerEnumeration *) enumeration;
    gce->joystickIndex = NextControllerIndex(gce->joystickIndex, gce->joystickCount);
    return EnumerationGet(enumeration);
}

static int NextControllerIndex(int current, int count) {
    if (current >= count) {
        return count;
    }
    int i = current + 1;
    while (!SDL_IsGameController(i) && i < count) {
        i++;
    }
    return i;
}