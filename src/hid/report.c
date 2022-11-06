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

#include "protobuf/pb_utils.h"

static int ComputeDelta(const uint8_t *previous, const uint8_t *current, size_t inputLen, size_t reportLen,
                        uint8_t *delta);

void IHS_HIDReportHolderInit(IHS_HIDReportHolder *holder, uint32_t deviceId) {
    chidmessage_from_remote__device_input_reports__device_input_report__init(&holder->report);
    PROTOBUF_C_SET_VALUE(holder->report, device, deviceId);
    IHS_BufferInit(&holder->dataBuffer, 256, 8192);
    IHS_ArrayListInit(&holder->reportItems, sizeof(CHIDDeviceInputReport));
    IHS_ArrayListInit(&holder->reportPointers, sizeof(CHIDDeviceInputReport *));
    holder->report.reports = holder->reportPointers.data;
    holder->reportLength = 0;
}

void IHS_HIDReportHolderDeinit(IHS_HIDReportHolder *holder) {
    holder->report.reports = NULL;
    IHS_ArrayListDeinit(&holder->reportPointers);
    IHS_ArrayListDeinit(&holder->reportItems);
    IHS_BufferClear(&holder->dataBuffer, true);
}

void IHS_HIDReportHolderSetReportLength(IHS_HIDReportHolder *holder, size_t reportLen) {
    holder->reportLength = reportLen;
}

void IHS_HIDReportHolderAddFull(IHS_HIDReportHolder *holder, const uint8_t *current, size_t len) {
    // TODO lock?
    uint8_t *data = IHS_BufferPointerForAppend(&holder->dataBuffer, len);
    assert(holder->reportLength >= len);
    memcpy(data, current, len);
    CHIDDeviceInputReport *item = IHS_ArrayListAppend(&holder->reportItems, NULL);
    chiddevice_input_report__init(item);
    item->has_full_report = true;
    item->full_report.data = data;
    item->full_report.len = len;

    holder->report.n_reports = holder->reportItems.size;
}

void IHS_HIDReportHolderAddDelta(IHS_HIDReportHolder *holder, const uint8_t *previous, const uint8_t *current,
                                 size_t len) {
    // TODO lock?
    uint8_t *data = IHS_BufferPointerForAppend(&holder->dataBuffer, holder->reportLength);
    int deltaLen = ComputeDelta(previous, current, len, holder->reportLength, data);
    // Send the data and CRC
    uint32_t crc = IHS_CRC32C(current, len);
    CHIDDeviceInputReport *item = IHS_ArrayListAppend(&holder->reportItems, NULL);
    IHS_ArrayListAppend(&holder->reportPointers, &item);
    chiddevice_input_report__init(item);
    item->has_delta_report = true;
    item->delta_report.data = data;
    item->delta_report.len = deltaLen;
    PROTOBUF_C_P_SET_VALUE(item, delta_report_crc, crc);
    PROTOBUF_C_P_SET_VALUE(item, delta_report_size, len);

    holder->report.n_reports = holder->reportItems.size;
}

IHS_HIDDeviceReportMessage *IHS_HIDReportHolderGetMessage(IHS_HIDReportHolder *holder) {
    if (holder->reportItems.size == 0) {
        return NULL;
    }
    return &holder->report;
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