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

#include <stdatomic.h>

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
     * Set when any device holder has a report waiting, cleared by the poll tick when it sends.
     * Written from whichever thread feeds input (the SDL event thread, for the SDL provider) and
     * read from the timer thread, with no lock in common — hence atomic.
     */
    atomic_bool reportsPending;
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
     * IHS_SessionHIDSendReport once if anything was added or `reportsPending` is set. This is the
     * only place reports are flushed, so a burst of input events collapses into one message per
     * tick. Created lazily on the first IHS_HIDManagerAddProvider call; destroyed in
     * IHS_HIDManagerDestroy.
     */
    IHS_TimerTask *pollTimer;
    /**
     * Feature reports the device refused, waiting to be retried. The reference retries a failing
     * send_feature_report 50 times 2 ms apart (OnRemoteHIDMessage @ 0x228a64 case 6) — but it does
     * so with a blocking sleep on its socket receive thread, and ihslib's equivalent thread also
     * drives every timer, so sleeping there would stall the whole session for up to 100 ms.
     * The retries are queued here and driven by `featureRetryTimer` instead.
     *
     * Only touched by the control message handler and by that timer, both of which run on the
     * session's worker thread, so no lock of its own.
     */
    IHS_ArrayList featureReports;
    /** Runs while `featureReports` is non-empty, then ends itself. */
    IHS_TimerTask *featureRetryTimer;
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

/**
 * Flag that a report is waiting to be flushed by the next poll tick.
 */
void IHS_HIDManagerMarkReportsPending(IHS_HIDManager *manager);

/**
 * Send a feature report, retrying if the device refuses it. Mirrors the retry the reference wraps
 * around this one call (50 attempts, 2 ms apart, until the device returns >= 0), except that the
 * waiting happens on a timer rather than by sleeping in the caller.
 *
 * Returns as soon as the first attempt is made. Nothing is reported back to the host either way —
 * the reference sends no response for this command, success or failure.
 */
void IHS_HIDManagerSendFeatureReport(IHS_HIDManager *manager, uint32_t deviceId, const uint8_t *data,
                                     size_t dataLen);

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
