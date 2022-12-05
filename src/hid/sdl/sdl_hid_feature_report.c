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

typedef enum EControllerType {
    k_ControllerTypeSteamController_a = 0x2,
    k_ControllerTypeSteamController_b = 0x3,
    k_ControllerTypeSteamController_c = 0x4,
    k_ControllerTypeGen = 0x1e,
    k_ControllerTypeXB360 = 0x1f,
    k_ControllerTypeXB1 = 0x20,
    k_ControllerTypePS3 = 0x21,
    k_ControllerTypePS4 = 0x22,
    k_ControllerTypeSwitch_a = 0x26,
    k_ControllerTypeSwitch_b = 0x27,
    k_ControllerTypeSwitch_c = 0x28,
    k_ControllerTypeSwitchGen = 0x2a,
    k_ControllerTypeMobileTouch = 0x2b,
    k_ControllerTypePS5 = 0x2d,
} EControllerType;

typedef struct __attribute__((__packed__)) {
    bool valid;
    bool xinput;
    EControllerType controllerType: 32;
    int32_t playerIndex: 32;
    bool hid;
    uint8_t pad[9];
} DeviceFeatureReport;

_Static_assert(sizeof(DeviceFeatureReport) == 20, "");

int IHS_HIDDeviceSDLGetFeatureReport(IHS_HIDDevice *device, const uint8_t *reportNumber, size_t reportNumberLen,
                                     IHS_Buffer *dest, size_t length) {
    IHS_HIDDeviceSDL *sdl = (IHS_HIDDeviceSDL *) device;
    switch (reportNumber[0]) {
        case 0x07: {
            IHS_BufferWriteMem(dest, 0, reportNumber, 1);
#if IHS_SDL_TARGET_ATLEAST(2, 0, 14)
            const char *serial = SDL_GameControllerGetSerial(sdl->controller);
            IHS_BufferWriteMem(dest, 1, (const uint8_t *) serial, strlen(serial) + 1);
#else
            // Write an empty string
            IHS_BufferFillMem(dest, 1, 0, 1);
#endif
            break;
        }
        case 0x02: {
            IHS_BufferWriteMem(dest, 0, reportNumber, 1);
#if IHS_SDL_TARGET_ATLEAST(2, 0, 4)
            SDL_Joystick *joystick = SDL_GameControllerGetJoystick(sdl->controller);
            uint8_t level = SDL_JoystickCurrentPowerLevel(joystick);
            IHS_BufferWriteMem(dest, 1, &level, 1);
#else
            // Write an empty string
            IHS_BufferFillMem(dest, 1, SDL_JOYSTICK_POWER_UNKNOWN, 1);
#endif
            break;
        }
        case 0x04: {
            int playerIndex = -1;
#if IHS_SDL_TARGET_ATLEAST(2, 0, 9)
            playerIndex = SDL_GameControllerGetPlayerIndex(sdl->controller);
#else
            playerIndex = sdl->playerIndex;
#endif

            DeviceFeatureReport report = {
                    .valid = true,
                    .xinput = false,
                    .playerIndex = playerIndex,
                    .controllerType = k_ControllerTypeGen,
                    .hid = false,
            };
            IHS_BufferWriteMem(dest, 0, reportNumber, 1);
            IHS_BufferWriteMem(dest, 1, (const unsigned char *) &report, 20);
            break;
        }
        default: {
            // Write an empty array
            IHS_BufferFillMem(dest, 0, 0, 1);
            return 0;
        }
    }
    return 0;
}