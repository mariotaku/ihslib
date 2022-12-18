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

#include <stdlib.h>
#include <assert.h>
#include "manager.h"
#include "device.h"
#include "provider.h"
#include "session/channels/ch_control.h"
#include "protobuf/pb_utils.h"
#include "session/session_pri.h"

static int CompareDeviceID(const uint32_t *id, const IHS_HIDManagedDevice *device);

IHS_HIDManager *IHS_HIDManagerCreate() {
    IHS_HIDManager *manager = calloc(1, sizeof(IHS_HIDManager));
    IHS_ArrayListInit(&manager->providers, sizeof(IHS_HIDProvider *));
    IHS_ArrayListInit(&manager->devices, sizeof(IHS_HIDManagedDevice));
    IHS_ArrayListInit(&manager->inputReports, sizeof(IHS_HIDDeviceReportMessage *));
    return manager;
}

void IHS_HIDManagerDestroy(IHS_HIDManager *manager) {
    for (size_t i = 0, j = manager->providers.size; i < j; ++i) {
        IHS_HIDProvider *provider = *((IHS_HIDProvider **) IHS_ArrayListGet(&manager->providers, i));
        assert(provider->session == manager->session);
        provider->session = NULL;
    }
    IHS_ArrayListDeinit(&manager->devices);
    IHS_ArrayListDeinit(&manager->providers);
    IHS_ArrayListDeinit(&manager->inputReports);
    free(manager);
}

void IHS_HIDManagerCloseAll(IHS_HIDManager *manager) {
    for (size_t i = 0, j = manager->devices.size; i < j; ++i) {
        IHS_HIDManagedDevice *managed = IHS_ArrayListGet(&manager->devices, i);
        IHS_HIDManagedDeviceClose(managed);
    }
    IHS_ArrayListClear(&manager->devices);
}

IHS_HIDManagedDevice *IHS_HIDManagerOpenDevice(IHS_HIDManager *manager, const char *path) {
    for (size_t i = 0, j = manager->providers.size; i < j; ++i) {
        IHS_HIDProvider *provider = *((IHS_HIDProvider **) IHS_ArrayListGet(&manager->providers, i));
        if (IHS_HIDProviderSupportsDevice(provider, path)) {
            IHS_HIDDevice *device = IHS_HIDProviderOpenDevice(provider, path);
            if (device == NULL) {
                continue;
            }
            IHS_HIDManagedDevice *managed = IHS_ArrayListAppend(&manager->devices, NULL);
            managed->id = ++manager->lastDeviceId;
            managed->manager = manager;
            managed->device = device;
            managed->lock = IHS_MutexCreate();
            IHS_HIDReportHolderInit(&managed->reportHolder, managed->id);
            device->managed = managed;
            IHS_HIDManagedDeviceOpened(managed);
            return managed;
        }
    }
    return NULL;
}

IHS_HIDManagedDevice *IHS_HIDManagerFindDeviceByID(IHS_HIDManager *manager, uint32_t id) {
    int index = IHS_ArrayListBinarySearch(&manager->devices, &id, (IHS_ArrayListSearchFn) CompareDeviceID);
    if (index < 0) {
        return NULL;
    }
    return IHS_ArrayListGet(&manager->devices, index);
}

IHS_HIDManagedDevice *IHS_HIDManagerFindDevice(IHS_HIDManager *manager, IHS_HIDDeviceComparator predicate,
                                               const void *value) {
    int index = IHS_ArrayListLinearSearch(&manager->devices, value, (IHS_ArrayListSearchFn) predicate);
    if (index < 0) {
        return NULL;
    }
    return IHS_ArrayListGet(&manager->devices, index);
}

bool IHS_HIDManagerNotifyDeviceClosed(IHS_HIDManager *manager, IHS_HIDManagedDevice *managed) {
    IHS_SessionChannel *channel = IHS_SessionChannelForType(manager->session, IHS_SessionChannelTypeControl);
    CHIDMessageFromRemote message = CHIDMESSAGE_FROM_REMOTE__INIT;
    CHIDMessageFromRemote__CloseDevice closeDevice = CHIDMESSAGE_FROM_REMOTE__CLOSE_DEVICE__INIT;
    PROTOBUF_C_SET_VALUE(closeDevice, device, managed->id);
    message.command_case = CHIDMESSAGE_FROM_REMOTE__COMMAND_CLOSE_DEVICE;
    message.close_device = &closeDevice;
    IHS_SessionLog(manager->session, IHS_LogLevelDebug, "HID", "Close device, id=%u", managed->id);
    return IHS_SessionChannelControlSendHIDMsg(channel, &message);
}

void IHS_HIDManagerRemoveClosedDevice(IHS_HIDManager *manager, IHS_HIDManagedDevice *managed) {
    int index = IHS_ArrayListBinarySearch(&manager->devices, &managed->id, (IHS_ArrayListSearchFn) CompareDeviceID);
    assert(index >= 0);
    IHS_HIDReportHolderDeinit(&managed->reportHolder);
    IHS_MutexDestroy(managed->lock);
    bool removed = IHS_ArrayListRemove(&manager->devices, index);
    assert(removed);
}

void IHS_HIDManagerAddProvider(IHS_HIDManager *manager, IHS_HIDProvider *provider) {
    provider->session = manager->session;
    IHS_ArrayListAppend(&manager->providers, &provider);
}

void IHS_HIDManagerRemoveProvider(IHS_HIDManager *manager, IHS_HIDProvider *provider) {
    bool removed = IHS_ArrayListRemoveFirst(&manager->providers, &provider);
    assert(removed);
}

static int CompareDeviceID(const uint32_t *id, const IHS_HIDManagedDevice *device) {
    return (int) (*id - device->id);
}