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

#pragma once

#include "ihslib/hid.h"
#include "ihslib/hid/sdl.h"

#include "sdl_hid_common.h"

typedef struct IHS_HIDDeviceSDLEnumerationClass {
    IHS_EnumerationClass base;
    bool (*getInfo)(IHS_Enumeration *enumeration, IHS_HIDDeviceInfo *info);
} IHS_HIDDeviceSDLEnumerationClass;

typedef struct GameControllerEnumeration {
    IHS_Enumeration base;

    int joystickIndex;
    int joystickCount;

    struct {
        char path[16];
        char product_string[64];
    } temp;
} GameControllerEnumeration;

#if IHS_SDL_TARGET_ATLEAST(2, 0, 6)

IHS_Enumeration *IHS_HIDDeviceSDLEnumerateManaged();

#endif

IHS_Enumeration *IHS_HIDDeviceSDLEnumerateUnmanaged(const IHS_HIDProviderSDLDeviceList *list, void *listContext);

bool IHS_HIDDeviceSDLEnumerationGetInfo(IHS_Enumeration *enumeration, IHS_HIDDeviceInfo *info);
