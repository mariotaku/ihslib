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
#include "sdl_hid_common.h"

#if IHS_SDL_TARGET_ATLEAST(2, 0, 6)

#include <stddef.h>
#include <stdlib.h>
#include "sdl_hid_utils.h"
#include "sdl_hid_enumerators.h"

#include "ihslib/enumeration.h"
#include "ihslib/hid.h"
#include "ihslib/hid/sdl.h"

static IHS_Enumeration *EnumerationAlloc(const IHS_EnumerationClass *cls, void *arg);

static size_t EnumerationCount(const IHS_Enumeration *enumeration);

static void EnumerationReset(IHS_Enumeration *enumeration);

static bool EnumerationEnded(const IHS_Enumeration *enumeration);

static void *EnumerationGet(const IHS_Enumeration *enumeration);

static void *EnumerationNext(IHS_Enumeration *enumeration);

static bool EnumerationGetInfo(IHS_Enumeration *enumeration, IHS_HIDDeviceInfo *info);

static int NextControllerIndex(int current, int count);

const static IHS_HIDDeviceSDLEnumerationClass ManagedEnumerationClass = {
        .base = {
                .alloc = EnumerationAlloc,
                .free = (void (*)(IHS_Enumeration *)) free,
                .count = EnumerationCount,
                .reset = EnumerationReset,
                .ended = EnumerationEnded,
                .get = EnumerationGet,
                .next = EnumerationNext,
        },
        .getInfo =EnumerationGetInfo,
};

IHS_Enumeration *IHS_HIDDeviceSDLEnumerateManaged() {
    return IHS_EnumerationCreate((const IHS_EnumerationClass *) &ManagedEnumerationClass, NULL);
}

static IHS_Enumeration *EnumerationAlloc(const IHS_EnumerationClass *cls, void *arg) {
    (void) arg;
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

static bool EnumerationGetInfo(IHS_Enumeration *enumeration, IHS_HIDDeviceInfo *info) {
    GameControllerEnumeration *gce = (GameControllerEnumeration *) enumeration;
    int index = gce->joystickIndex;
    SDL_JoystickID instanceId = SDL_JoystickGetDeviceInstanceID(index);
    if (instanceId == -1) {
        return false;
    }
    snprintf(gce->temp.path, 16, "sdl://%d", instanceId);
    const char *name = SDL_JoystickNameForIndex(index);
    if (name != NULL) {
        strncpy(gce->temp.product_string, name, 63);
        gce->temp.product_string[63] = '\0';
    } else {
        strcpy(gce->temp.product_string, "Generic Gamepad");
    }
    info->vendor_id = SDL_JoystickGetDeviceVendor(index);
    info->product_id = SDL_JoystickGetDeviceProduct(index);
    info->product_version = SDL_JoystickGetDeviceProductVersion(index);
    info->path = gce->temp.path;
    info->product_string = gce->temp.product_string;
    return true;
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

#endif