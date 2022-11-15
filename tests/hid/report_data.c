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
#include <string.h>
#include "hid/report.h"

int main() {
    // Nothing is pressed
    uint8_t a[48] = {
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    };
    // Menu is pressed
    uint8_t b[48] = {
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    };

    IHS_HIDReportHolder holder;
    IHS_HIDReportHolderInit(&holder, 3);

    IHS_HIDReportHolderSetReportLength(&holder, 65);

    assert(IHS_HIDReportHolderGetMessage(&holder) == NULL);

    IHS_HIDReportHolderAddFull(&holder, a, 48);
    IHS_HIDReportHolderAddDelta(&holder, a, b, 48);

    IHS_HIDDeviceReportMessage *report = IHS_HIDReportHolderGetMessage(&holder);
    assert(report->has_device);
    assert(report->device == 3);
    assert(report->n_reports == 2);
    assert(report->reports[0]->full_report.len == 48);
    assert(memcmp(report->reports[0]->full_report.data, a, 48) == 0);

    assert(report->reports[1]->delta_report_size == 48);
    assert(report->reports[1]->delta_report.len == 10);
    uint8_t deltaExpected[] = {0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40};
    assert(memcmp(report->reports[1]->delta_report.data, deltaExpected, 10) == 0);
    assert(report->reports[1]->delta_report_crc == 406293423);

    IHS_HIDReportHolderDeinit(&holder);
}