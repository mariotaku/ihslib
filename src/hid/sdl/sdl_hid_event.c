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

#include "ihslib/hid/sdl.h"

#include "hid/manager.h"

#include "sdl_hid_common.h"
#include "session/session_pri.h"

static bool HandleRemoveEvent(IHS_HIDManager *manager, const SDL_ControllerDeviceEvent *event);

static bool HandleCButtonEvent(IHS_HIDManager *manager, const SDL_ControllerButtonEvent *event);

static bool HandleCAxisEvent(IHS_HIDManager *manager, const SDL_ControllerAxisEvent *event);

bool IHS_HIDHandleSDLEvent(IHS_Session *session, const SDL_Event *event) {
    switch (event->type) {
        case SDL_CONTROLLERDEVICEADDED: {
            IHS_SessionHIDNotifyDeviceChange(session);
            return true;
        }
        case SDL_CONTROLLERDEVICEREMOVED: {
            bool changed = HandleRemoveEvent(session->hidManager, &event->cdevice);
            IHS_SessionHIDNotifyDeviceChange(session);
            return changed;
        }
        case SDL_CONTROLLERBUTTONDOWN:
        case SDL_CONTROLLERBUTTONUP: {
            bool changed = HandleCButtonEvent(session->hidManager, &event->cbutton);
            if (changed) {
                IHS_SessionHIDSendReport(session);
            }
            return changed;
        }
        case SDL_CONTROLLERAXISMOTION: {
            bool changed = HandleCAxisEvent(session->hidManager, &event->caxis);
            if (changed) {
                IHS_SessionHIDSendReport(session);
            }
            return changed;
        }
    }
    return false;
}

bool IHS_HIDResetSDLGameControllers(IHS_Session *session) {
    IHS_HIDManager *manager = session->hidManager;
    bool changed = false;
    for (size_t i = 0, j = manager->devices.size; i < j; ++i) {
        IHS_HIDManagedDevice *managed = IHS_ArrayListGet(&manager->devices, i);
        if (!IHS_HIDDeviceIsSDL(managed->device)) {
            continue;
        }
        IHS_HIDDeviceSDL *device = (IHS_HIDDeviceSDL *) managed->device;
        IHS_HIDDeviceLock(managed->device);
        if (IHS_HIDReportSDLClear(&device->states.current)) {
            changed = true;
            IHS_HIDDeviceReportAddDelta(managed->device, (const uint8_t *) &device->states.previous,
                                        (const uint8_t *) &device->states.current, 48);
            device->states.previous = device->states.current;
        }
        IHS_HIDDeviceUnlock(managed->device);
    }
    if (changed) {
        IHS_SessionHIDSendReport(session);
    }
    return true;
}

static bool HandleRemoveEvent(IHS_HIDManager *manager, const SDL_ControllerDeviceEvent *event) {
    IHS_HIDManagedDevice *managed = IHS_HIDManagerDeviceByJoystickID(manager, event->which);
    if (managed == NULL) {
        return false;
    }
    IHS_HIDManagedDeviceClose(managed);
    return true;
}

static bool HandleCButtonEvent(IHS_HIDManager *manager, const SDL_ControllerButtonEvent *event) {
    IHS_HIDManagedDevice *managed = IHS_HIDManagerDeviceByJoystickID(manager, event->which);
    if (managed == NULL) {
        return false;
    }
    IHS_HIDDeviceSDL *device = (IHS_HIDDeviceSDL *) managed->device;
    assert(device != NULL);
    IHS_HIDDeviceLock(managed->device);
    bool changed = IHS_HIDReportSDLSetButton(&device->states.current, event->button,
                                             event->state == SDL_PRESSED);
    if (changed) {
        IHS_HIDDeviceReportAddDelta(managed->device, (const uint8_t *) &device->states.previous,
                                    (const uint8_t *) &device->states.current, 48);
        device->states.previous = device->states.current;
    }
    IHS_HIDDeviceUnlock(managed->device);
    return changed;
}

static bool HandleCAxisEvent(IHS_HIDManager *manager, const SDL_ControllerAxisEvent *event) {
    IHS_HIDManagedDevice *managed = IHS_HIDManagerDeviceByJoystickID(manager, event->which);
    if (managed == NULL) {
        return false;
    }
    IHS_HIDDeviceSDL *device = (IHS_HIDDeviceSDL *) managed->device;
    assert(device != NULL);
    IHS_HIDDeviceLock(managed->device);
    bool changed = IHS_HIDReportSDLSetAxis(&device->states.current, event->axis, event->value);
    if (changed) {
        IHS_HIDDeviceReportAddDelta(managed->device, (const uint8_t *) &device->states.previous,
                                    (const uint8_t *) &device->states.current, 48);
        device->states.previous = device->states.current;
    }
    IHS_HIDDeviceUnlock(managed->device);
    return changed;
}