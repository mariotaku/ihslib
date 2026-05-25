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
#include "hid/sdl/src/sdl_hid_report.h"

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

    // Regression: SetAccel previously wrote into report->gyro[] instead of report->accel[].
    float accel[3] = {1.0f, 2.0f, 3.0f};
    float gyro[3] = {-1.0f, -2.0f, -3.0f};
    assert(IHS_HIDReportSDLSetAccel(&report, accel));
    // Gyro must remain untouched by SetAccel.
    assert(report.gyro[0] == 0 && report.gyro[1] == 0 && report.gyro[2] == 0);
    // Accel must contain the remapped values, not zero.
    assert(report.accel[0] != 0 && report.accel[1] != 0 && report.accel[2] != 0);
    int16_t accelBefore[3] = {report.accel[0], report.accel[1], report.accel[2]};
    // SetGyro must update gyro and leave accel intact.
    assert(IHS_HIDReportSDLSetGyro(&report, gyro));
    assert(report.gyro[0] != 0 && report.gyro[1] != 0 && report.gyro[2] != 0);
    assert(report.accel[0] == accelBefore[0]);
    assert(report.accel[1] == accelBefore[1]);
    assert(report.accel[2] == accelBefore[2]);

    return 0;
}