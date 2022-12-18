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

#include "ihslib/buffer.h"
#include "ihslib/hid.h"

#include "sdl_hid_common.h"
#include "session/session_pri.h"

#include <SDL2/SDL.h>

static IHS_HIDDevice *DeviceAlloc(const struct IHS_HIDDeviceClass *cls);

static void DeviceOpened(IHS_HIDDevice *device);

static void DeviceFree(IHS_HIDDevice *device);

static void DeviceClose(IHS_HIDDevice *device);

static int DeviceRead(IHS_HIDDevice *device, IHS_Buffer *dest, size_t length, uint32_t timeoutMs);

static int DeviceSendFeatureReport(IHS_HIDDevice *device, const uint8_t *data, size_t dataLen);

static int DeviceVendorString(IHS_HIDDevice *device, IHS_Buffer *dest);

static int DeviceProductString(IHS_HIDDevice *device, IHS_Buffer *dest);

static int DeviceSerialNumber(IHS_HIDDevice *device, IHS_Buffer *dest);

static int DeviceStartInputReports(IHS_HIDDevice *device, size_t length);

static int DeviceRequestFullReport(IHS_HIDDevice *device);

static int DeviceRequestDisconnect(IHS_HIDDevice *device, int method, const uint8_t *data, size_t dataLen);

static const IHS_HIDDeviceClass DeviceClass = {
        .alloc = DeviceAlloc,
        .opened = DeviceOpened,
        .free = DeviceFree,
        .close = DeviceClose,
        .write = IHS_HIDDeviceSDLWrite,
        .read = DeviceRead,
        .sendFeatureReport = DeviceSendFeatureReport,
        .getFeatureReport = IHS_HIDDeviceSDLGetFeatureReport,
        .getVendorString = DeviceVendorString,
        .getProductString = DeviceProductString,
        .getSerialNumberString = DeviceSerialNumber,
        .startInputReports = DeviceStartInputReports,
        .requestFullReport = DeviceRequestFullReport,
        .requestDisconnect = DeviceRequestDisconnect,
};

IHS_HIDDevice *IHS_HIDDeviceSDLCreate(IHS_HIDProvider *provider, SDL_GameController *controller, bool managed) {
    IHS_HIDDeviceSDL *device = (IHS_HIDDeviceSDL *) IHS_HIDDeviceCreate(&DeviceClass);
    SDL_Joystick *joystick = SDL_GameControllerGetJoystick(controller);
    device->controllerManaged = managed;
    device->instanceId = SDL_JoystickInstanceID(joystick);
    device->controller = controller;
    device->playerIndex = -1;
    device->hapticEffectId = -1;
    SDL_Haptic *haptic = SDL_HapticOpenFromJoystick(joystick);
    if (haptic != NULL) {
        unsigned int hapticBits = SDL_HapticQuery(haptic);
        if (hapticBits & SDL_HAPTIC_LEFTRIGHT) {
            device->haptic = haptic;
        } else {
            SDL_HapticClose(haptic);
        }
    } else {
        IHS_SessionLog(provider->session, IHS_LogLevelWarn, "HID.SDL", "Device haptic is not supported: %s",
                       SDL_GetError());
    }
    return (IHS_HIDDevice *) device;
}

bool IHS_HIDDeviceIsSDL(const IHS_HIDDevice *device) {
    return device->cls == &DeviceClass;
}

static IHS_HIDDevice *DeviceAlloc(const IHS_HIDDeviceClass *cls) {
    IHS_HIDDeviceSDL *device = calloc(1, sizeof(IHS_HIDDeviceSDL));
    device->base.cls = cls;
    return (IHS_HIDDevice *) device;
}

static void DeviceOpened(IHS_HIDDevice *device) {
    IHS_HIDDeviceSDL *deviceSdl = (IHS_HIDDeviceSDL *) device;
    IHS_HIDReportSDLInit(&deviceSdl->states.previous);
    IHS_HIDReportSDLInit(&deviceSdl->states.current);
}

static void DeviceFree(IHS_HIDDevice *device) {
    free(device);
}

static void DeviceClose(IHS_HIDDevice *device) {
    IHS_HIDDeviceSDL *deviceSdl = (IHS_HIDDeviceSDL *) device;
    if (deviceSdl->haptic != NULL) {
        if (deviceSdl->hapticEffectId != -1) {
            SDL_HapticDestroyEffect(deviceSdl->haptic, deviceSdl->hapticEffectId);
        }
        SDL_HapticClose(deviceSdl->haptic);
        deviceSdl->haptic = NULL;
    }

    if (deviceSdl->controllerManaged) {
        SDL_GameControllerClose(deviceSdl->controller);
        deviceSdl->controller = NULL;
    }
}

static int DeviceRead(IHS_HIDDevice *device, IHS_Buffer *dest, size_t length, uint32_t timeoutMs) {
    (void) device;
    (void) timeoutMs;
    uint8_t *pointer = IHS_BufferPointerForAppend(dest, length);
    memset(pointer, 0, length);
    return (int) length;
}

static int DeviceSendFeatureReport(IHS_HIDDevice *device, const uint8_t *data, size_t dataLen) {
    (void) device;
    (void) data;
    (void) dataLen;
    // Should return -1 according to official implementation
    return -1;
}

static int DeviceVendorString(IHS_HIDDevice *device, IHS_Buffer *dest) {
    (void) device;
    IHS_BufferWriteMem(dest, 0, (const unsigned char *) "", 1);
    return 0;
}

static int DeviceProductString(IHS_HIDDevice *device, IHS_Buffer *dest) {
    (void) device;
    IHS_BufferWriteMem(dest, 0, (const unsigned char *) "", 1);
    return 0;
}

static int DeviceSerialNumber(IHS_HIDDevice *device, IHS_Buffer *dest) {
#if IHS_SDL_TARGET_ATLEAST(2, 0, 14)
    IHS_HIDDeviceSDL *sdl = (IHS_HIDDeviceSDL *) device;
    const char *serial = SDL_GameControllerGetSerial(sdl->controller);
    if (serial != NULL) {
        IHS_BufferWriteMem(dest, 0, (const unsigned char *) serial, strlen(serial));
    }
#endif
    IHS_BufferWriteMem(dest, 0, (const unsigned char *) "", 1);
    return 0;
}

static int DeviceStartInputReports(IHS_HIDDevice *device, size_t length) {
    (void) length;
    IHS_HIDDeviceSDL *sdl = (IHS_HIDDeviceSDL *) device;
    IHS_HIDDeviceLock(device);
    IHS_HIDDeviceReportAddFull(device, (const uint8_t *) &sdl->states.current, 48);
    sdl->states.previous = sdl->states.current;
    IHS_HIDDeviceUnlock(device);
    return 0;
}

static int DeviceRequestFullReport(IHS_HIDDevice *device) {
    IHS_HIDDeviceSDL *sdl = (IHS_HIDDeviceSDL *) device;
    IHS_HIDDeviceLock(device);
    IHS_HIDDeviceReportAddFull(device, (const uint8_t *) &sdl->states.current, 48);
    sdl->states.previous = sdl->states.current;
    IHS_HIDDeviceUnlock(device);
    return 0;
}

static int DeviceRequestDisconnect(IHS_HIDDevice *device, int method, const uint8_t *data, size_t dataLen) {
    return 0;
}