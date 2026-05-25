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

#include "ihslib/hid.h"
#include "ihs_arraylist.h"
#include "ihs_thread.h"
#include "ihs_timer.h"
#include "report.h"

typedef struct IHS_HIDDevice IHS_HIDDevice;

typedef struct IHS_HIDManagedDevice IHS_HIDManagedDevice;

struct IHS_HIDManager {
    IHS_Session *session;
    /**
     * Stores `IHS_HIDManagedDevice *` (not the struct). Each slot owns one managed device
     * for the lifetime of the manager: once a device is closed its slot's `closed` flag
     * flips true, but the slot stays in place so any concurrent holder of the pointer it
     * returned from a Find* call sees stable memory. All reads/writes of this list are
     * protected by `devicesLock`.
     */
    IHS_ArrayList devices;
    IHS_Mutex *devicesLock;
    IHS_ArrayList providers;
    IHS_ArrayList inputReports;
    uint32_t lastDeviceId;
    /**
     * 125 Hz poll task that drains every device whose class implements `poll`, then calls
     * IHS_SessionHIDSendReport once if anything was added. Created lazily on the first
     * IHS_HIDManagerAddProvider call; destroyed in IHS_HIDManagerDestroy.
     */
    IHS_TimerTask *pollTimer;
};

struct IHS_HIDManagedDevice {
    IHS_HIDDevice *device;
    IHS_HIDManager *manager;
    uint32_t id;
    IHS_HIDReportHolder reportHolder;
    IHS_Mutex *lock;
    /**
     * Set true by IHS_HIDManagerRemoveClosedDevice. Find* skip closed slots; the slot
     * (and this struct) live until IHS_HIDManagerDestroy reclaims them — that keeps every
     * IHS_HIDManagedDevice * stable for the lifetime of the manager.
     */
    bool closed;
};


/**
 * @param value Value to compare
 * @param device Pointer to the entry in devices list
 * @return 0 if matched
 */
typedef int(*IHS_HIDDeviceComparator)(const void *value, const IHS_HIDDevice **device);

IHS_HIDManager *IHS_HIDManagerCreate();

void IHS_HIDManagerDestroy(IHS_HIDManager *manager);

void IHS_HIDManagerCloseAll(IHS_HIDManager *manager);

IHS_HIDManagedDevice *IHS_HIDManagerOpenDevice(IHS_HIDManager *manager, const char *path);

IHS_HIDManagedDevice *IHS_HIDManagerFindDeviceByID(IHS_HIDManager *manager, uint32_t id);

IHS_HIDManagedDevice *IHS_HIDManagerFindDevice(IHS_HIDManager *manager, IHS_HIDDeviceComparator predicate,
                                               const void *value);

bool IHS_HIDManagerNotifyDeviceClosed(IHS_HIDManager *manager, IHS_HIDManagedDevice *managed);

void IHS_HIDManagerRemoveClosedDevice(IHS_HIDManager *manager, IHS_HIDManagedDevice *managed);

void IHS_HIDManagerAddProvider(IHS_HIDManager *manager, IHS_HIDProvider *provider);

void IHS_HIDManagerRemoveProvider(IHS_HIDManager *manager, IHS_HIDProvider *provider);

/**
 * Walk the device list under `devicesLock` and copy every still-open managed device
 * pointer into a freshly-malloc'd array. Returns NULL with `*count = 0` when there are
 * no open devices. Caller owns the returned array and must `free()` it.
 *
 * This is the only sanctioned way to iterate devices from outside `manager.c` — direct
 * traversal of `manager->devices` would race with concurrent Open/Close on other threads.
 */
IHS_HIDManagedDevice **IHS_HIDManagerSnapshotOpenDevices(IHS_HIDManager *manager, size_t *count);
