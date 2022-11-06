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

#include "ihs_buffer.h"
#include "ihslib/hid.h"

#include "hid/device.h"

#include "sdl_hid_common.h"
#include "crc32c.h"

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

IHS_HIDDevice *IHS_HIDDeviceSDLCreate(SDL_GameController *controller) {
    IHS_HIDDeviceSDL *device = (IHS_HIDDeviceSDL *) IHS_HIDDeviceCreate(&DeviceClass);
    device->controller = controller;
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
}

static void DeviceFree(IHS_HIDDevice *device) {
    free(device);
}

static void DeviceClose(IHS_HIDDevice *device) {
    IHS_HIDDeviceSDL *deviceSdl = (IHS_HIDDeviceSDL *) device;
    SDL_GameControllerClose(deviceSdl->controller);
    deviceSdl->controller = NULL;
}

static int DeviceRead(IHS_HIDDevice *device, IHS_Buffer *dest, size_t length, uint32_t timeoutMs) {
    (void) device;
    (void) timeoutMs;
    uint8_t *pointer = IHS_BufferPointerForAppend(dest, length);
    memset(pointer, 0, length);
    return (int) length;
}

static int DeviceSendFeatureReport(IHS_HIDDevice *device, const uint8_t *data, size_t dataLen) {
    // Should return -1 according to official implementation
    return -1;
}

static int DeviceVendorString(IHS_HIDDevice *device, IHS_Buffer *dest) {
    IHS_BufferWriteMem(dest, 0, (const unsigned char *) "", 1);
    return 0;
}

static int DeviceProductString(IHS_HIDDevice *device, IHS_Buffer *dest) {
    IHS_BufferWriteMem(dest, 0, (const unsigned char *) "", 1);
    return 0;
}

static int DeviceSerialNumber(IHS_HIDDevice *device, IHS_Buffer *dest) {
#if SDL_VERSION_ATLEAST(2, 0, 14)
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
    IHS_HIDDeviceSDL *sdl = (IHS_HIDDeviceSDL *) device;
    sdl->states.previous = sdl->states.current;
    return 0;
}

static int DeviceRequestFullReport(IHS_HIDDevice *device) {
    IHS_HIDDeviceSDL *sdl = (IHS_HIDDeviceSDL *) device;
    sdl->states.previous = sdl->states.current;
    return 0;
}

static int DeviceRequestDisconnect(IHS_HIDDevice *device, int method, const uint8_t *data, size_t dataLen) {

}