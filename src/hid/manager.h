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

typedef struct IHS_HIDDevice IHS_HIDDevice;

struct IHS_HIDManager {
    IHS_ArrayList devices;
    IHS_ArrayList providers;
    uint32_t lastDeviceId;
};

IHS_HIDManager *IHS_HIDManagerCreate();

void IHS_HIDManagerDestroy(IHS_HIDManager *manager);

IHS_HIDDevice *IHS_HIDManagerOpenDevice(IHS_HIDManager *manager, const char *path);

IHS_HIDDevice *IHS_HIDManagerFindDevice(IHS_HIDManager *manager, uint32_t id);

void IHS_HIDManagerRemoveClosedDevice(IHS_HIDManager *manager, IHS_HIDDevice *device);

void IHS_HIDManagerAddProvider(IHS_HIDManager *manager, IHS_HIDProvider *provider);

void IHS_HIDManagerRemoveProvider(IHS_HIDManager *manager, IHS_HIDProvider *provider);