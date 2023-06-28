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

#include <stdint.h>
#include <stdbool.h>

#include <SDL2/SDL.h>

typedef struct __attribute__((__packed__)) IHS_HIDReportSDL {
    int16_t axes[6];
    uint32_t flags;
    uint16_t buttons;
    uint8_t padding1[9];
    uint8_t reportUnknown;
    int16_t gyro[3];
    int16_t accel[3];
    int16_t touch[2];
    uint8_t padding2[4];
} IHS_HIDStateSDL;

_Static_assert(sizeof(IHS_HIDStateSDL) == 48, "");

bool IHS_HIDReportSDLInit(IHS_HIDStateSDL *report);

bool IHS_HIDReportSDLSetRequestedReportVersion(IHS_HIDStateSDL *report, uint8_t version);

bool IHS_HIDReportSDLSetButton(IHS_HIDStateSDL *report, SDL_GameControllerButton button, bool pressed);

bool IHS_HIDReportSDLSetAxis(IHS_HIDStateSDL *report, SDL_GameControllerAxis axis, int16_t value);

bool IHS_HIDReportSDLSetAccel(IHS_HIDStateSDL *report, const float accel[3]);

bool IHS_HIDReportSDLSetGyro(IHS_HIDStateSDL *report, const float gyro[3]);

bool IHS_HIDReportSDLClear(IHS_HIDStateSDL *report);