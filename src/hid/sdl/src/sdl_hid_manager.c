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

#include "sdl_hid_common.h"

#include "hid/manager.h"

static int CompareDeviceByJoystickID(const SDL_JoystickID *id, const IHS_HIDDevice **deviceListPtr);

IHS_HIDManagedDevice *IHS_HIDManagerDeviceByJoystickID(IHS_HIDManager *manager, SDL_JoystickID joystickId) {
    return IHS_HIDManagerFindDevice(manager, (IHS_HIDDeviceComparator) CompareDeviceByJoystickID, &joystickId);
}

static int CompareDeviceByJoystickID(const SDL_JoystickID *id, const IHS_HIDDevice **deviceListPtr) {
    const IHS_HIDDevice *device = *deviceListPtr;
    if (!IHS_HIDDeviceIsSDL(device)) {
        return -1;
    }
    const IHS_HIDDeviceSDL *deviceSdl = (const IHS_HIDDeviceSDL *) device;
    return *id - deviceSdl->instanceId;
}