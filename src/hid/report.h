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

#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include "ihs_buffer.h"
#include "ihs_arraylist.h"

#include "protobuf/hiddevices.pb-c.h"

typedef struct IHS_HIDDevice IHS_HIDDevice;

typedef CHIDMessageFromRemote__DeviceInputReports__DeviceInputReport IHS_HIDDeviceReportMessage;

/**
 * DeviceInputReport message has many pointers to manage inside, this structure is to hold them.
 */
typedef struct IHS_HIDReportHolder {
    /**
     * Reused device report
     */
    IHS_HIDDeviceReportMessage report;
    /**
     * Continuous buffer storing all the data referenced in CHIDDeviceInputReport.
     */
    IHS_Buffer dataBuffer;
    /**
     * List storing report items (CHIDDeviceInputReport).
     */
    IHS_ArrayList reportItems;
    /**
     * List of (CHIDDeviceInputReport*). Rebound by GetMessage: reportItems reallocates as it
     * grows, so a pointer taken at Add time goes stale.
     */
    IHS_ArrayList reportPointers;
    /**
     * Per-item offset (size_t) into dataBuffer. dataBuffer reallocates too, so the report data
     * pointers are bound to it only at GetMessage time.
     */
    IHS_ArrayList reportOffsets;
    /**
     * Data length for single report item. Will be used for delta calculation, etc.
     */
    size_t reportLength;
    /**
     * Last raw input state actually flushed to the wire. Used for identical-state dedup
     * (mirrors CHIDDeviceReportGenerator::SendBuffer's memcmp(buf, lastBuffer) drop).
     * Allocated lazily on the first AddDelta/AddFull call.
     */
    uint8_t *lastSent;
    size_t lastSentLen;
    /**
     * Most recent raw state passed to AddDelta/AddFull since the last send. Promoted to
     * lastSent by ResetMessage after a successful send.
     */
    uint8_t *pendingCurrent;
    size_t pendingCurrentLen;
    size_t bufferCapacity;
    /**
     * Set when the host asked for a full report, cleared only once one is actually appended.
     * While set, the identical-state dedup must not drop and delta encoding must not be used —
     * mirrors CHIDDeviceReportGenerator's fullReportUnsent latch (RequestFullReport @ 0x20c840,
     * cleared in the set_full_report branch of SendBuffer @ 0x2454ec).
     */
    bool fullReportPending;
} IHS_HIDReportHolder;

void IHS_HIDReportHolderInit(IHS_HIDReportHolder *holder, uint32_t deviceId);

void IHS_HIDReportHolderDeinit(IHS_HIDReportHolder *holder);

void IHS_HIDReportHolderSetReportLength(IHS_HIDReportHolder *holder, size_t reportLen);

/**
 * Latch a full-report request. The next appended report will be a full one, and the identical-state
 * dedup will not swallow it.
 */
void IHS_HIDReportHolderRequestFullReport(IHS_HIDReportHolder *holder);

void IHS_HIDReportHolderAddFull(IHS_HIDReportHolder *holder, const uint8_t *current, size_t len);

void IHS_HIDReportHolderAddDelta(IHS_HIDReportHolder *holder, const uint8_t *previous, const uint8_t *current,
                                 size_t len);

/**
 *
 * @return Pointer for input report, or NULL if there is no report item. Please lock the holder to prevent it being
 * modified during usage
 */
IHS_HIDDeviceReportMessage *IHS_HIDReportHolderGetMessage(IHS_HIDReportHolder *holder);

void IHS_HIDReportHolderResetMessage(IHS_HIDReportHolder *holder);