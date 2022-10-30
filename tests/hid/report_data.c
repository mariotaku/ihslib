#include <assert.h>
#include <string.h>
#include "hid/report.h"

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

int main() {
    uint8_t a[8] = {0x0, 0x1, 0x0, 0x1, 0x0,};
    uint8_t b[8] = {0x1, 0x2, 0x0, 0x1, 0x1,};

    IHS_HIDReportHolder holder;
    IHS_HIDReportHolderInit(&holder, 3);

    IHS_HIDReportHolderSetLength(&holder, 16);
    uint8_t *bufAddr = holder.deltaBuf;
    assert(bufAddr != NULL);

    // Subsequent calls should have no effect
    IHS_HIDReportHolderSetLength(&holder, 16);
    assert(bufAddr == holder.deltaBuf);

    IHS_HIDReportHolderUpdateDelta(&holder, a, b, 8);

    assert(holder.report.has_device);
    assert(holder.report.device == 3);
    assert(holder.report.n_reports == 1);
    assert(holder.report.reports[0]->delta_report.len == 5);

    uint8_t deltaExpected[] = {0x13 /*0b00011001*/, 0x0, 0x1, 0x2, 0x1};
    assert(memcmp(holder.report.reports[0]->delta_report.data, deltaExpected, 5) == 0);

    IHS_HIDReportHolderDeinit(&holder);
}