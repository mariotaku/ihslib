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
#include "sdl_hid_utils.h"

typedef enum GetFeatureReportCommand {
    GetFeatureReportGetPowerLevel = 0x02,
    GetFeatureReportGetCaps = 0x04,
    GetFeatureReportGetSerial = 0x07,
} GetFeatureReportCommand;

/**
 * @see https://github.com/libsdl-org/SDL/blob/53dea9830964eee8b5c2a7ee0a65d6e268dc78a1/src/joystick/controller_type.h
 */
typedef enum EControllerType {
    k_ControllerTypeNone = -1,
    k_ControllerTypeUnknown = 0,

    // Steam Controllers
    k_ControllerTypeUnknownSteamController = 1,
    k_ControllerTypeSteamController = 2,
    k_ControllerTypeSteamControllerV2 = 3,

    // Other Controllers
    k_ControllerTypeUnknownNonSteamController = 30,
    k_ControllerTypeXBox360Controller = 31,
    k_ControllerTypeXBoxOneController = 32,
    k_ControllerTypePS3Controller = 33,
    k_ControllerTypePS4Controller = 34,
    k_ControllerTypeWiiController = 35,
    k_ControllerTypeAppleController = 36,
    k_ControllerTypeAndroidController = 37,
    k_ControllerTypeSwitchProController = 38,
    k_ControllerTypeSwitchJoyConLeft = 39,
    k_ControllerTypeSwitchJoyConRight = 40,
    k_ControllerTypeSwitchJoyConPair = 41,
    k_ControllerTypeSwitchInputOnlyController = 42,
    k_ControllerTypeMobileTouch = 43,
    k_ControllerTypeXInputSwitchController = 44,  // Client-side only, used to mark Switch-compatible controllers as not supporting Switch controller protocol
    k_ControllerTypePS5Controller = 45,
    k_ControllerTypeXboxEliteController = 46,
    k_ControllerTypeLastController, // Don't add game controllers below this enumeration - this enumeration can change value
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

bool IsXinputDevice(const SDL_JoystickGUID *guid);

bool IsHIDAPIDevice(const SDL_JoystickGUID *guid);

EControllerType InferControllerType(const SDL_JoystickGUID *guid);

int IHS_HIDDeviceSDLGetFeatureReport(IHS_HIDDevice *device, const uint8_t *reportNumber, size_t reportNumberLen,
                                     IHS_Buffer *dest, size_t length) {
    IHS_HIDDeviceSDL *sdl = (IHS_HIDDeviceSDL *) device;
    switch (reportNumber[0]) {
        case GetFeatureReportGetSerial: {
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
        case GetFeatureReportGetPowerLevel: {
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
        case GetFeatureReportGetCaps: {
            int playerIndex = -1;
            SDL_JoystickGUID guid = SDL_JoystickGetGUID(SDL_GameControllerGetJoystick(sdl->controller));

            bool xinput = IsXinputDevice(&guid);
            if (!xinput) {
#if IHS_SDL_TARGET_ATLEAST(2, 0, 9)
                playerIndex = SDL_GameControllerGetPlayerIndex(sdl->controller);
#else
                playerIndex = sdl->playerIndex;
#endif
            }
            DeviceFeatureReport report = {
                    .valid = true,
                    .xinput = xinput,
                    .playerIndex = playerIndex,
                    .controllerType = InferControllerType(&guid),
                    .hid = IsHIDAPIDevice(&guid),
            };
            IHS_BufferWriteMem(dest, 0, reportNumber, 1);
            IHS_BufferWriteMem(dest, 1, (const unsigned char *) &report, 20);
            break;
        }
        default: {
            // Write an empty array
            IHS_BufferFillMem(dest, 1, 0, 1);
            return 0;
        }
    }
    return 0;
}

bool IsXinputDevice(const SDL_JoystickGUID *guid) {
    return guid->data[14] == 'x';
}

bool IsHIDAPIDevice(const SDL_JoystickGUID *guid) {
    return guid->data[14] == 'h';
}

enum UsbVendorId {
    UsbVendorIdMicrosoft = 0x045e,
    UsbVendorIdSony = 0x054c,
    UsbVendorIdNintendo = 0x057e,
    UsbVendorIdValve = 0x28de,
};

EControllerType InferControllerType(const SDL_JoystickGUID *guid) {
    uint16_t vendorId, productId;
    if (!IHS_HIDDeviceSDLGetJoystickGUIDInfo(guid, &vendorId, &productId, NULL, NULL)) {
        return k_ControllerTypeUnknownNonSteamController;
    }
    switch (vendorId) {
        case UsbVendorIdMicrosoft: {
            return k_ControllerTypeXBoxOneController;
        }
        case UsbVendorIdSony: {
            return k_ControllerTypePS4Controller;
        }
        case UsbVendorIdNintendo: {
            return k_ControllerTypeXInputSwitchController;
        }
        case UsbVendorIdValve: {
            return k_ControllerTypeSteamController;
        }
        default: {
            return k_ControllerTypeUnknownNonSteamController;
        }
    }
}