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
#include <assert.h>
#include "hid/sdl/sdl_hid_report.h"

int main() {
    IHS_HIDStateSDL report;
    IHS_HIDReportSDLInit(&report);
    IHS_HIDReportSDLSetRequestedReportVersion(&report, 1);

    assert(!IHS_HIDReportSDLSetButton(&report, SDL_CONTROLLER_BUTTON_PADDLE1, true));
    assert(IHS_HIDReportSDLSetButton(&report, SDL_CONTROLLER_BUTTON_A, true));
    assert(IHS_HIDReportSDLSetButton(&report, SDL_CONTROLLER_BUTTON_DPAD_RIGHT, true));

    int8_t buttonsExpected[] = {0x01, 0x40};
    assert(memcmp(((int8_t *) &report) + 16, buttonsExpected, 2) == 0);

    assert(IHS_HIDReportSDLSetButton(&report, SDL_CONTROLLER_BUTTON_A, false));
    assert(IHS_HIDReportSDLSetButton(&report, SDL_CONTROLLER_BUTTON_DPAD_RIGHT, false));

    assert(!IHS_HIDReportSDLSetAxis(&report, SDL_CONTROLLER_AXIS_MAX, 0));
    assert(IHS_HIDReportSDLSetAxis(&report, SDL_CONTROLLER_AXIS_LEFTX, INT16_MAX));

    return 0;
}