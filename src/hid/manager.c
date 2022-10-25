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

#include "ihslib/hid.h"

static int CompareDeviceID(const uint32_t *id, const IHS_HIDDevice **device);

IHS_HIDManager *IHS_HIDManagerCreate() {
    IHS_HIDManager *manager = calloc(1, sizeof(IHS_HIDManager));
    IHS_ArrayListInit(&manager->providers, sizeof(IHS_HIDProvider *));
    IHS_ArrayListInit(&manager->devices, sizeof(IHS_HIDDevice *));
    return manager;
}

void IHS_HIDManagerDestroy(IHS_HIDManager *manager) {
    IHS_ArrayListDeinit(&manager->devices);
    IHS_ArrayListDeinit(&manager->providers);
    free(manager);
}

IHS_HIDDevice *IHS_HIDManagerOpenDevice(IHS_HIDManager *manager, const char *path) {
    for (size_t i = 0, j = manager->providers.size; i < j; ++i) {
        IHS_HIDProvider *provider = *((IHS_HIDProvider **) IHS_ArrayListGet(&manager->providers, i));
        if (IHS_HIDProviderSupportsDevice(provider, path)) {
            IHS_HIDDevice *device = IHS_HIDProviderOpenDevice(provider, path);
            if (device == NULL) {
                continue;
            }
            device->id = ++manager->lastDeviceId;
            device->manager = manager;
            IHS_ArrayListAppend(&manager->devices, &device);
            return device;
        }
    }
    return NULL;
}

IHS_HIDDevice *IHS_HIDManagerFindDeviceByID(IHS_HIDManager *manager, uint32_t id) {
    int index = IHS_ArrayListBinarySearch(&manager->devices, &id, (IHS_ArrayListSearchFn) CompareDeviceID);
    if (index < 0) {
        return NULL;
    }
    IHS_HIDDevice **itemPtr = IHS_ArrayListGet(&manager->devices, index);
    return *itemPtr;
}

IHS_HIDDevice *IHS_HIDManagerFindDevice(IHS_HIDManager *manager, IHS_HIDDeviceComparator predicate, const void *value) {
    int index = IHS_ArrayListLinearSearch(&manager->devices, value, (IHS_ArrayListSearchFn) predicate);
    if (index < 0) {
        return NULL;
    }
    IHS_HIDDevice **itemPtr = IHS_ArrayListGet(&manager->devices, index);
    return *itemPtr;
}

void IHS_HIDManagerRemoveClosedDevice(IHS_HIDManager *manager, IHS_HIDDevice *device) {
    int index = IHS_ArrayListBinarySearch(&manager->devices, &device->id, (IHS_ArrayListSearchFn) CompareDeviceID);
    assert(index >= 0);
}

void IHS_HIDManagerAddProvider(IHS_HIDManager *manager, IHS_HIDProvider *provider) {
    IHS_ArrayListAppend(&manager->providers, &provider);
}

void IHS_HIDManagerRemoveProvider(IHS_HIDManager *manager, IHS_HIDProvider *provider) {
    bool removed = IHS_ArrayListRemoveFirst(&manager->providers, &provider);
    assert(removed);
}

static int CompareDeviceID(const uint32_t *id, const IHS_HIDDevice **device) {
    return (int) (*id - (*device)->id);
}