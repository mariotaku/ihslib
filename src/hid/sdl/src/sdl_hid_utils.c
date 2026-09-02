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

#include "sdl_hid_utils.h"
#include "sdl_hid_common.h"
#include "ihslib/hid.h"

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
/** What an SDL_GameController mapping guarantees, and all that can be said without an open handle. */
#define SDL_GAMEPAD_BASE_CAPS (IHS_HID_CAP_ABXY | IHS_HID_CAP_DPAD | IHS_HID_CAP_LSTICK | IHS_HID_CAP_RSTICK)

uint32_t IHS_HIDDeviceSDLCaps(SDL_GameController *controller) {
    if (controller == NULL) {
        return SDL_GAMEPAD_BASE_CAPS;
    }
#if IHS_HID_SDL_TARGET_ATLEAST(2, 0, 14)
    uint32_t caps = 0;
    if (SDL_GameControllerHasButton(controller, SDL_CONTROLLER_BUTTON_A) &&
        SDL_GameControllerHasButton(controller, SDL_CONTROLLER_BUTTON_B) &&
        SDL_GameControllerHasButton(controller, SDL_CONTROLLER_BUTTON_X) &&
        SDL_GameControllerHasButton(controller, SDL_CONTROLLER_BUTTON_Y)) {
        caps |= IHS_HID_CAP_ABXY;
    }
    if (SDL_GameControllerHasButton(controller, SDL_CONTROLLER_BUTTON_DPAD_UP) &&
        SDL_GameControllerHasButton(controller, SDL_CONTROLLER_BUTTON_DPAD_DOWN) &&
        SDL_GameControllerHasButton(controller, SDL_CONTROLLER_BUTTON_DPAD_LEFT) &&
        SDL_GameControllerHasButton(controller, SDL_CONTROLLER_BUTTON_DPAD_RIGHT)) {
        caps |= IHS_HID_CAP_DPAD;
    }
    if (SDL_GameControllerHasAxis(controller, SDL_CONTROLLER_AXIS_LEFTX) &&
        SDL_GameControllerHasAxis(controller, SDL_CONTROLLER_AXIS_LEFTY)) {
        caps |= IHS_HID_CAP_LSTICK;
    }
    if (SDL_GameControllerHasAxis(controller, SDL_CONTROLLER_AXIS_RIGHTX) &&
        SDL_GameControllerHasAxis(controller, SDL_CONTROLLER_AXIS_RIGHTY)) {
        caps |= IHS_HID_CAP_RSTICK;
    }
    // The reference tests each pair with OR, not AND: one stick button, one shoulder or one trigger
    // is enough to claim the capability.
    if (SDL_GameControllerHasButton(controller, SDL_CONTROLLER_BUTTON_LEFTSTICK) ||
        SDL_GameControllerHasButton(controller, SDL_CONTROLLER_BUTTON_RIGHTSTICK)) {
        caps |= IHS_HID_CAP_STICKBTNS;
    }
    if (SDL_GameControllerHasButton(controller, SDL_CONTROLLER_BUTTON_LEFTSHOULDER) ||
        SDL_GameControllerHasButton(controller, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER)) {
        caps |= IHS_HID_CAP_SHOULDERS;
    }
    if (SDL_GameControllerHasAxis(controller, SDL_CONTROLLER_AXIS_TRIGGERLEFT) ||
        SDL_GameControllerHasAxis(controller, SDL_CONTROLLER_AXIS_TRIGGERRIGHT)) {
        caps |= IHS_HID_CAP_TRIGGERS;
    }
    if (SDL_GameControllerHasButton(controller, SDL_CONTROLLER_BUTTON_BACK)) {
        caps |= IHS_HID_CAP_BACK;
    }
    if (SDL_GameControllerHasButton(controller, SDL_CONTROLLER_BUTTON_START)) {
        caps |= IHS_HID_CAP_START;
    }
    if (SDL_GameControllerHasButton(controller, SDL_CONTROLLER_BUTTON_GUIDE)) {
        caps |= IHS_HID_CAP_GUIDE;
    }
    if (SDL_GameControllerHasButton(controller, SDL_CONTROLLER_BUTTON_MISC1)) {
        caps |= IHS_HID_CAP_MISC_1;
    }
    bool upperPaddles = SDL_GameControllerHasButton(controller, SDL_CONTROLLER_BUTTON_PADDLE1) ||
                        SDL_GameControllerHasButton(controller, SDL_CONTROLLER_BUTTON_PADDLE2);
    bool lowerPaddles = SDL_GameControllerHasButton(controller, SDL_CONTROLLER_BUTTON_PADDLE3) ||
                        SDL_GameControllerHasButton(controller, SDL_CONTROLLER_BUTTON_PADDLE4);
    if (upperPaddles || lowerPaddles) {
        caps |= IHS_HID_CAP_PADDLES;
    }
    if (upperPaddles && lowerPaddles) {
        caps |= IHS_HID_CAP_PADDLES_4;
    }
    if (SDL_GameControllerHasLED(controller)) {
        // SDL2 cannot say whether the LED takes a colour, so IHS_HID_CAP_LED_RGB is left unset
        // rather than guessed. The 2022 reference asserted both for an Xbox Elite Series 2, which
        // has neither.
        caps |= IHS_HID_CAP_LED;
    }
    if (SDL_GameControllerHasSensor(controller, SDL_SENSOR_GYRO)) {
        caps |= IHS_HID_CAP_GYRO | IHS_HID_CAP_MOTION;
    }
#if IHS_HID_SDL_TARGET_ATLEAST(2, 0, 18)
    if (SDL_GameControllerHasRumble(controller)) {
        caps |= IHS_HID_CAP_RUMBLE;
    }
#else
    // Before SDL_GameControllerHasRumble existed, the reference's own stand-in was the GUID driver
    // tag: an XInput or HIDAPI device was assumed to rumble.
    caps |= IHS_HID_CAP_RUMBLE;
#endif
    return caps;
#else
    (void) controller;
    return SDL_GAMEPAD_BASE_CAPS;
#endif
}
