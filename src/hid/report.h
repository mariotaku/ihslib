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
#include <stddef.h>

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
     * List of (CHIDDeviceInputReport*).
     */
    IHS_ArrayList reportPointers;
    /**
     * Data length for single report item. Will be used for delta calculation, etc.
     */
    size_t reportLength;
} IHS_HIDReportHolder;

void IHS_HIDReportHolderInit(IHS_HIDReportHolder *holder, uint32_t deviceId);

void IHS_HIDReportHolderDeinit(IHS_HIDReportHolder *holder);

void IHS_HIDReportHolderSetReportLength(IHS_HIDReportHolder *holder, size_t reportLen);

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