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

IHS_HIDProvider *IHS_SessionHIDProviderCreate(const IHS_HIDProviderClass *cls);

void IHS_SessionHIDProviderDestroy(IHS_HIDProvider *);

bool IHS_HIDProviderSupportsDevice(IHS_HIDProvider *provider, const char *path);

IHS_HIDDevice *IHS_HIDProviderOpenDevice(IHS_HIDProvider *provider, const char *path);

bool IHS_SessionHIDProviderHasChange(IHS_HIDProvider *provider);

IHS_Enumeration *IHS_HIDProviderEnumerateDevices(IHS_HIDProvider *provider);

void IHS_HIDProviderDeviceInfo(IHS_HIDProvider *provider, IHS_Enumeration *enumeration,
                               IHS_HIDDeviceInfo *info);