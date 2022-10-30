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

#include "report.h"
#include "crc32c.h"

#include <memory.h>
#include <stdbool.h>
#include <malloc.h>

#include "protobuf/pb_utils.h"

static int ComputeDelta(const uint8_t *previous, const uint8_t *current, size_t inputLen, size_t reportLen,
                        uint8_t *delta);

void IHS_HIDReportHolderInit(IHS_HIDReportHolder *holder, uint32_t deviceId) {
    holder->deltaBuf = NULL;
    holder->reportLen = 0;
    chidmessage_from_remote__device_input_reports__device_input_report__init(&holder->report);
    PROTOBUF_C_SET_VALUE(holder->report, device, deviceId);
    holder->reportItems[0] = &holder->reportItem;
    holder->report.n_reports = 1;
    holder->report.reports = holder->reportItems;
}

void IHS_HIDReportHolderDeinit(IHS_HIDReportHolder *holder) {
    if (holder->deltaBuf != NULL) {
        free(holder->deltaBuf);
    }
}

void IHS_HIDReportHolderSetLength(IHS_HIDReportHolder *holder, size_t len) {
    if (holder->reportLen == len) {
        return;
    }
    holder->reportLen = len;
    holder->deltaBuf = realloc(holder->deltaBuf, len);
}

void IHS_HIDReportHolderUpdateDelta(IHS_HIDReportHolder *holder, const uint8_t *previous, const uint8_t *current,
                                    size_t len) {
    int deltaLen = ComputeDelta(previous, current, len, holder->reportLen, holder->deltaBuf);
    // Send the data and CRC
    uint32_t crc = IHS_CRC32C(current, len);

    CHIDDeviceInputReport report = CHIDDEVICE_INPUT_REPORT__INIT;
    report.has_delta_report = true;
    report.delta_report.data = holder->deltaBuf;
    report.delta_report.len = deltaLen;
    PROTOBUF_C_SET_VALUE(report, delta_report_crc, crc);

    holder->reportItem = report;
}

static int ComputeDelta(const uint8_t *previous, const uint8_t *current, size_t inputLen, size_t reportLen,
                        uint8_t *delta) {
    int size = (int) (reportLen + 7) >> 3;
    memset(delta, 0, size);
    for (size_t i = 0; i < inputLen; ++i) {
        if (previous[i] == current[i]) {
            continue;
        }
        delta[i >> 3] |= 1 << (i % 8);
        delta[size] = current[i];
        size++;
    }
    return size;
}