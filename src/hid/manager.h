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

#include "ihslib/hid.h"
#include "ihs_arraylist.h"
#include "ihs_thread.h"
#include "report.h"

#include "protobuf/hiddevices.pb-c.h"

typedef struct IHS_HIDDevice IHS_HIDDevice;

typedef struct IHS_HIDManagedDevice IHS_HIDManagedDevice;

struct IHS_HIDManager {
    IHS_ArrayList devices;
    IHS_ArrayList providers;
    IHS_ArrayList inputReports;
    uint32_t lastDeviceId;
};

struct IHS_HIDManagedDevice {
    /**
     * @attention Must be the first field!
     */
    IHS_HIDDevice *device;
    IHS_HIDManager *manager;
    uint32_t id;
    IHS_HIDReportHolder reportHolder;
    IHS_Mutex *lock;
};


/**
 * @param value Value to compare
 * @param device Pointer to the entry in devices list
 * @return 0 if matched
 */
typedef int(*IHS_HIDDeviceComparator)(const void *value, const IHS_HIDDevice **device);

IHS_HIDManager *IHS_HIDManagerCreate();

void IHS_HIDManagerDestroy(IHS_HIDManager *manager);

IHS_HIDManagedDevice *IHS_HIDManagerOpenDevice(IHS_HIDManager *manager, const char *path);

IHS_HIDManagedDevice *IHS_HIDManagerFindDeviceByID(IHS_HIDManager *manager, uint32_t id);

IHS_HIDManagedDevice *IHS_HIDManagerFindDevice(IHS_HIDManager *manager, IHS_HIDDeviceComparator predicate,
                                        const void *value);

void IHS_HIDManagerRemoveClosedDevice(IHS_HIDManager *manager, IHS_HIDManagedDevice *managed);

void IHS_HIDManagerAddProvider(IHS_HIDManager *manager, IHS_HIDProvider *provider);

void IHS_HIDManagerRemoveProvider(IHS_HIDManager *manager, IHS_HIDProvider *provider);
