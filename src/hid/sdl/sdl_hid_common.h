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

#include "ihslib/hid/sdl.h"

#include <stdint.h>
#include <stdbool.h>

#include <SDL2/SDL.h>

#include "hid/device.h"

#include "sdl_hid_report.h"
#include "hid/report.h"

typedef struct IHS_HIDDeviceSDL {
    IHS_HIDDevice base;
    /**
     * If true, the controller will be closed when the device is closed
     */
    bool controllerManaged;
    SDL_JoystickID instanceId;
    SDL_GameController *controller;
    int playerIndex;
#if !IHS_HID_SDL_TARGET_ATLEAST(2, 0, 9)
    SDL_Haptic *haptic;
    int hapticEffectId;
#endif
    struct {
        IHS_HIDStateSDL current;
        IHS_HIDStateSDL previous;
    } states;
} IHS_HIDDeviceSDL;

IHS_HIDDevice *IHS_HIDDeviceSDLCreate(IHS_HIDProvider *provider, SDL_GameController *controller, bool managed);

bool IHS_HIDDeviceIsSDL(const IHS_HIDDevice *device);

int IHS_HIDDeviceSDLWrite(IHS_HIDDevice *device, const uint8_t *data, size_t dataLen);

int IHS_HIDDeviceSDLGetFeatureReport(IHS_HIDDevice *device, const uint8_t *reportNumber, size_t reportNumberLen,
                                     IHS_Buffer *dest, size_t length);

IHS_HIDManagedDevice *IHS_HIDManagerDeviceByJoystickID(IHS_HIDManager *manager, SDL_JoystickID joystickId);