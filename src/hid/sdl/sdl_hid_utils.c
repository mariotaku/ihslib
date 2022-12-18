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

#include "sdl_hid_utils.h"

bool IHS_HIDDeviceSDLGetJoystickGUIDInfo(const SDL_JoystickGUID *guid, Uint16 *vendor, Uint16 *product, Uint16 *version,
                                         Uint16 *crc16) {
    const Uint16 *guid16 = (const Uint16 *) guid->data;
    Uint16 bus = SDL_SwapLE16(guid16[0]);

    if (bus < ' ' && guid16[3] == 0x0000 && guid16[5] == 0x0000) {
        /* This GUID fits the standard form:
         * 16-bit bus
         * 16-bit CRC16 of the joystick name (can be zero)
         * 16-bit vendor ID
         * 16-bit zero
         * 16-bit product ID
         * 16-bit zero
         * 16-bit version
         * 8-bit driver identifier ('h' for HIDAPI, 'x' for XInput, etc.)
         * 8-bit driver-dependent type info
         */
        if (vendor) {
            *vendor = SDL_SwapLE16(guid16[2]);
        }
        if (product) {
            *product = SDL_SwapLE16(guid16[4]);
        }
        if (version) {
            *version = SDL_SwapLE16(guid16[6]);
        }
        if (crc16) {
            *crc16 = SDL_SwapLE16(guid16[1]);
        }
    } else {
        return false;
    }
    return true;
}