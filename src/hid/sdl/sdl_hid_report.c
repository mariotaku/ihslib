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

#include "sdl_hid_report.h"

static float RemapValClamped(float val, float A, float B, float C, float D);

bool IHS_HIDReportSDLInit(IHS_HIDStateSDL *report) {
    memset(report, 0, sizeof(IHS_HIDStateSDL));
    return true;
}

bool IHS_HIDReportSDLSetRequestedReportVersion(IHS_HIDStateSDL *report, uint8_t version) {
    report->reportUnknown = version;
    return true;
}

bool IHS_HIDReportSDLSetButton(IHS_HIDStateSDL *report, SDL_GameControllerButton button, bool pressed) {
    if (button < 0 || button >= 16) {
        return false;
    }
    uint16_t prev = report->buttons;
    if (pressed) {
        report->buttons |= 1 << button;
    } else {
        report->buttons &= ~(1 << button);
    }
    return prev != report->buttons;
}

bool IHS_HIDReportSDLSetAxis(IHS_HIDStateSDL *report, SDL_GameControllerAxis axis, int16_t value) {
    if (axis < 0 || axis >= 6) {
        return false;
    }
    int16_t prev = report->axes[axis];
    report->axes[axis] = value;
    return prev != value;
}

bool IHS_HIDReportSDLSetAccel(IHS_HIDStateSDL *report, const float accel[3]) {
    bool changed = false;
    for (int i = 0; i < 3; i++) {
        int16_t newValue = (int16_t) RemapValClamped(accel[i], -19.6133f, 19.6133f, -32768.0f, 32767.0f);
        if (report->accel[i] == newValue) {
            continue;
        }
        report->gyro[i] = newValue;
        changed = true;
    }
    return changed;
}

bool IHS_HIDReportSDLSetGyro(IHS_HIDStateSDL *report, const float gyro[3]) {
    bool changed = false;
    for (int i = 0; i < 3; i++) {
        int16_t newValue = (int16_t) RemapValClamped(gyro[i], -34.90659f, 34.90659f, -32768.0f, 32767.0f);
        if (report->gyro[i] == newValue) {
            continue;
        }
        report->gyro[i] = newValue;
        changed = true;
    }
    return changed;
}

bool IHS_HIDReportSDLClear(IHS_HIDStateSDL *report) {
    uint8_t version = report->reportUnknown;
    memset(report, 0, sizeof(IHS_HIDStateSDL));
    report->reportUnknown = version;
    return true;
}

static float RemapValClamped(float val, float A, float B, float C, float D) {
    if (A == B) {
        return (val - B) >= 0.0f ? D : C;
    } else {
        float cVal = (val - A) / (B - A);
        cVal = SDL_clamp(cVal, 0.0f, 1.0f);

        return C + (D - C) * cVal;
    }
}