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

#include <assert.h>
#include <stdlib.h>
#include "provider.h"

IHS_HIDProvider *IHS_SessionHIDProviderCreate(const IHS_StreamHIDProviderClass *cls) {
    IHS_HIDProvider *provider = cls->alloc(cls);
    assert(provider->cls == cls);
    return provider;
}

void IHS_SessionHIDProviderDestroy(IHS_HIDProvider *provider) {
    provider->cls->free(provider);
}

bool IHS_HIDProviderSupportsDevice(IHS_HIDProvider *provider, const char *path) {
    return provider->cls->supportsDevice(provider, path);
}

IHS_HIDDevice *IHS_HIDProviderOpenDevice(IHS_HIDProvider *provider, const char *path) {
    return provider->cls->openDevice(provider, path);
}

bool IHS_SessionHIDProviderHasChange(IHS_HIDProvider *provider) {
    return provider->cls->hasChange(provider);
}

IHS_Enumeration *IHS_HIDProviderEnumerateDevices(IHS_HIDProvider *provider) {
    return provider->cls->enumerateDevices(provider);
}

void IHS_HIDProviderDeviceInfo(IHS_HIDProvider *provider, IHS_Enumeration *enumeration,
                               IHS_StreamHIDDeviceInfo *info) {
    provider->cls->deviceInfo(provider, enumeration, info);
}