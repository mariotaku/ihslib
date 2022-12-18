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

#include "sdl_hid_common.h"

typedef enum WriteCommandType {
    COMMAND_RUMBLE = 0x01,
    COMMAND_SET_LED = 0x05,
    COMMAND_RUMBLE_TRIGGERS = 0x06,
    COMMAND_SET_SENSOR_ENABLED = 0x08,
    COMMAND_SET_REQUESTED_REPORT_VERSION = 0x09,
    COMMAND_SET_PS5_RUMBLE = 0x0a,
    COMMAND_SET_PLAYER_INDEX = 0x0b,
} WriteCommandType;

typedef struct __attribute__((__packed__)) {
    WriteCommandType type: 8;
    uint16_t lowFreq: 16;
    uint16_t highFreq: 16;
    uint32_t durationMs: 32;
} RumbleCommand;

_Static_assert(sizeof(RumbleCommand) * 8 == 72, "");

typedef struct __attribute__((__packed__)) {
    WriteCommandType type: 8;
    uint8_t r: 8;
    uint8_t g: 8;
    uint8_t b: 8;
} LEDCommand;

_Static_assert(sizeof(LEDCommand) * 8 == 32, "");

typedef struct __attribute__((__packed__)) {
    WriteCommandType type: 8;
    uint8_t value: 8;
} ByteCommand;

_Static_assert(sizeof(ByteCommand) * 8 == 16, "");

typedef union __attribute__((__packed__))  WriteCommand {
    WriteCommandType type: 8;
    RumbleCommand rumble;
    LEDCommand led;
    ByteCommand byte;
} WriteCommand;

int IHS_HIDDeviceSDLWrite(IHS_HIDDevice *device, const uint8_t *data, size_t dataLen) {
    IHS_HIDDeviceSDL *sdl = (IHS_HIDDeviceSDL *) device;
    if (dataLen < sizeof(WriteCommand)) {
        return -1;
    }
    const WriteCommand *command = (const WriteCommand *) data;
    switch (command->type) {
        case COMMAND_RUMBLE: {
#if IHS_SDL_TARGET_ATLEAST(2, 0, 9)
            SDL_GameControllerRumble(sdl->controller, command->rumble.lowFreq,
                                     command->rumble.highFreq, command->rumble.durationMs);
#else
            if (sdl->haptic == NULL) {
                break;
            }
            SDL_HapticEffect effect = {.leftright = {
                    .type = SDL_HAPTIC_LEFTRIGHT,
                    .length = command->rumble.durationMs,
                    .large_magnitude = command->rumble.lowFreq / 2,
                    .small_magnitude = command->rumble.highFreq / 2,
            }};
            if (sdl->hapticEffectId != -1) {
                SDL_HapticUpdateEffect(sdl->haptic, sdl->hapticEffectId, &effect);
            } else {
                sdl->hapticEffectId = SDL_HapticNewEffect(sdl->haptic, &effect);
            }
            if (sdl->hapticEffectId != -1) {
                SDL_HapticRunEffect(sdl->haptic, sdl->hapticEffectId, 1);
            }
#endif
            break;
        }
        case COMMAND_SET_LED: {
#if IHS_SDL_TARGET_ATLEAST(2, 0, 14)
            SDL_GameControllerSetLED(sdl->controller, command->led.r, command->led.g, command->led.b);
#endif
            break;
        }
        case COMMAND_RUMBLE_TRIGGERS: {
#if IHS_SDL_TARGET_ATLEAST(2, 0, 14)
            SDL_GameControllerRumbleTriggers(sdl->controller, command->rumble.lowFreq,
                                             command->rumble.highFreq, command->rumble.durationMs);
#endif
            break;
        }
        case COMMAND_SET_SENSOR_ENABLED: {
#if IHS_SDL_TARGET_ATLEAST(2, 0, 14)
            SDL_GameControllerSetSensorEnabled(sdl->controller, SDL_SENSOR_ACCEL, command->byte.value);
            SDL_GameControllerSetSensorEnabled(sdl->controller, SDL_SENSOR_GYRO, command->byte.value);
#endif
            break;
        }
        case COMMAND_SET_REQUESTED_REPORT_VERSION: {
            IHS_HIDReportSDLSetRequestedReportVersion(&sdl->states.current, command->byte.value);
            break;
        }
        case COMMAND_SET_PS5_RUMBLE: {
#if IHS_SDL_TARGET_ATLEAST(2, 0, 16)
            SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_PS5_RUMBLE, command->byte.value ? "1" : "0");
#endif
            break;
        }
        case COMMAND_SET_PLAYER_INDEX: {
#if IHS_SDL_TARGET_ATLEAST(2, 0, 9)
            SDL_GameControllerSetPlayerIndex(sdl->controller, command->byte.value);
#endif
            sdl->playerIndex = command->byte.value;
            break;
        }
        default:
            return -1;
    }
    return 0;
}
