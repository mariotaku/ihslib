/*
 *  _____  _   _  _____  _  _  _
 * |_   _|| | | |/  ___|| |(_)| |     Steam
 *   | |  | |_| |\ `--. | | _ | |__     In-Home
 *   | |  |  _  | `--. \| || || '_ \      Streaming
 *  _| |_ | | | |/\__/ /| || || |_) |       Library
 *  \___/ \_| |_/\____/ |_||_||_.__/
 *
 * Copyright (c) 2022 Mariotaku <https://github.com/mariotaku>.
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

#include <SDL2/SDL.h>

#include <stdbool.h>
#include <stdint.h>

bool IHS_HIDDeviceSDLGetJoystickGUIDInfo(const SDL_JoystickGUID *guid, Uint16 *vendor, Uint16 *product, Uint16 *version,
                                         Uint16 *crc16);
/**
 * Capability bits for a controller, mirroring what CHIDDeviceListSDL derives from the SDL mapping
 * string — but asked of SDL directly where the SDL being built against can answer, rather than
 * assumed. IHS_HID_CAPS_ALWAYS is added later, by the code that announces the device.
 *
 * @param controller May be NULL, for a device that is enumerated but not open. Nothing can be
 *                   queried then, so a plain gamepad superset is reported, which is what ihslib
 *                   reported for every device before this existed.
 */
uint32_t IHS_HIDDeviceSDLCaps(SDL_GameController *controller);
