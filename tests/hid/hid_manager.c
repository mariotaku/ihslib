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
#include <assert.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>

#include "hid/manager.h"
#include "hid/provider.h"
#include "hid/device.h"
#include "ihs_enumeration.h"

#include "test_session.h"

static IHS_HIDProvider *ProviderAlloc(const IHS_HIDProviderClass *cls);

static void ProviderFree(IHS_HIDProvider *provider);

static bool ProviderSupportsDevice(IHS_HIDProvider *provider, const char *path);

static IHS_HIDDevice *ProviderOpenDevice(IHS_HIDProvider *provider, const char *path);

static IHS_Enumeration *ProviderEnumerate(IHS_HIDProvider *provider);

static const IHS_HIDProviderClass ProviderClass = {
        .alloc = ProviderAlloc,
        .free = ProviderFree,
        .supportsDevice = ProviderSupportsDevice,
        .openDevice = ProviderOpenDevice,
        .enumerateDevices = ProviderEnumerate,
};


static IHS_HIDDevice *DeviceAlloc(const struct IHS_HIDDeviceClass *cls);

static void DeviceFree(IHS_HIDDevice *device);

static void DeviceClose(IHS_HIDDevice *device);

static int DeviceWrite(IHS_HIDDevice *device, const uint8_t *data, size_t dataLen);

static int DeviceRead(IHS_HIDDevice *device, IHS_Buffer *dest, size_t length, uint32_t timeoutMs);

static int DeviceVendorString(IHS_HIDDevice *device, IHS_Buffer *dest);

static int DeviceProductString(IHS_HIDDevice *device, IHS_Buffer *dest);

static int DeviceSerialNumber(IHS_HIDDevice *device, IHS_Buffer *dest);

static const IHS_HIDDeviceClass DeviceClass = {
        .alloc = DeviceAlloc,
        .free = DeviceFree,
        .close = DeviceClose,
        .write = DeviceWrite,
        .read = DeviceRead,
        .getVendorString = DeviceVendorString,
        .getProductString = DeviceProductString,
        .getSerialNumberString = DeviceSerialNumber,
};


// Input events no longer send directly — they stash a report and flag the manager, and the 8 ms
// poll tick is the only thing that flushes. Check the flag tracks what actually landed in a holder.
static void TestReportsPendingFlag(IHS_HIDManager *manager, IHS_HIDManagedDevice *managed) {
    IHS_HIDDevice *device = managed->device;
    uint8_t state[48] = {0};
    state[27] = 0x01;

    IHS_HIDReportHolderSetReportLength(&managed->reportHolder, 48);
    assert(!atomic_load(&manager->reportsPending));

    // A report lands -> the tick has something to do.
    IHS_HIDDeviceReportAddFull(device, state, sizeof(state));
    assert(atomic_load(&manager->reportsPending));

    // Simulate the tick flushing.
    atomic_store(&manager->reportsPending, false);
    IHS_HIDReportHolderResetMessage(&managed->reportHolder);

    // The identical state is dropped by the dedup, so there is nothing to wake the tick for.
    IHS_HIDDeviceReportAddFull(device, state, sizeof(state));
    assert(!atomic_load(&manager->reportsPending));

    // A real change does wake it.
    uint8_t changed[48] = {0};
    memcpy(changed, state, sizeof(state));
    changed[16] = 0x40;
    IHS_HIDDeviceReportAddDelta(device, state, changed, sizeof(changed));
    assert(atomic_load(&manager->reportsPending));

    atomic_store(&manager->reportsPending, false);
    IHS_HIDReportHolderResetMessage(&managed->reportHolder);
}

int main() {
    IHS_Init();
    IHS_Session *session = IHS_TestSessionCreate();
    IHS_HIDManager *manager = session->hidManager;
    IHS_HIDProvider *provider = IHS_SessionHIDProviderCreate(&ProviderClass);
    IHS_HIDManagerAddProvider(manager, provider);

    IHS_Enumeration *enumeration = IHS_HIDProviderEnumerateDevices(provider);
    IHS_EnumerationFree(enumeration);

    assert(IHS_HIDManagerOpenDevice(manager, "sdl://0") == NULL);
    IHS_HIDManagedDevice *managed = IHS_HIDManagerOpenDevice(manager, "test://0");
    assert(managed != NULL);
    assert(managed->id == 1);
    assert(managed == IHS_HIDManagerFindDeviceByID(manager, 1));
    assert(IHS_HIDManagerFindDeviceByID(manager, 114514) == NULL);

    IHS_Buffer str = IHS_BUFFER_INIT(256, 256);

    IHS_HIDDevice *device = managed->device;

    IHS_HIDDeviceGetVendorString(device, &str);
    assert(strcmp("Vendor String", (const char *) IHS_BufferPointer(&str)) == 0);

    IHS_HIDDeviceGetProductString(device, &str);
    assert(strcmp("Product String", (const char *) IHS_BufferPointer(&str)) == 0);

    IHS_HIDDeviceGetSerialNumberString(device, &str);
    assert(strcmp("0123456789abcdef", (const char *) IHS_BufferPointer(&str)) == 0);


    IHS_Buffer buffer = IHS_BUFFER_INIT(256, 256);
    IHS_HIDDeviceRead(device, &buffer, 64, 5);
    IHS_HIDDeviceWrite(device, IHS_BufferPointer(&buffer), 64);

    TestReportsPendingFlag(manager, managed);

    IHS_HIDManagedDeviceClose(managed);

    IHS_HIDManagerRemoveProvider(manager, provider);
    assert(manager->providers.size == 0);
    IHS_SessionHIDProviderDestroy(provider);

    IHS_SessionDestroy(session);

    IHS_BufferClear(&str, true);
    IHS_BufferClear(&buffer, true);
    IHS_Quit();
    return 0;
}


static IHS_HIDProvider *ProviderAlloc(const IHS_HIDProviderClass *cls) {
    IHS_HIDProvider *provider = calloc(1, sizeof(IHS_HIDProvider));
    provider->cls = cls;
    return provider;
}

static void ProviderFree(IHS_HIDProvider *provider) {
    free(provider);
}

static bool ProviderSupportsDevice(IHS_HIDProvider *provider, const char *path) {
    return strstr(path, "test://") == path;
}

static IHS_HIDDevice *ProviderOpenDevice(IHS_HIDProvider *provider, const char *path) {
    return IHS_HIDDeviceCreate(&DeviceClass);
}

static IHS_Enumeration *ProviderEnumerate(IHS_HIDProvider *provider) {
    static int array[] = {1, 2, 3, 4};
    return IHS_EnumerationArrayCreate(array, sizeof(int), 4, NULL);
}

static IHS_HIDDevice *DeviceAlloc(const IHS_HIDDeviceClass *cls) {
    IHS_HIDDevice *device = calloc(1, sizeof(IHS_HIDDevice));
    device->cls = cls;
    return device;
}

static void DeviceFree(IHS_HIDDevice *device) {
    free(device);
}

static void DeviceClose(IHS_HIDDevice *device) {
}

static int DeviceWrite(IHS_HIDDevice *device, const uint8_t *data, size_t dataLen) {
    return 0;
}

static int DeviceRead(IHS_HIDDevice *device, IHS_Buffer *dest, size_t length, uint32_t timeoutMs) {
    (void) device;
    (void) timeoutMs;
    uint8_t *pointer = IHS_BufferPointerForAppend(dest, length);
    memset(pointer, 0, length);
    dest->size += length;
    return (int) length;
}

static int DeviceVendorString(IHS_HIDDevice *device, IHS_Buffer *dest) {
    char *src = "Vendor String";
    IHS_BufferWriteMem(dest, 0, (unsigned char *) src, strlen(src) + 1);
    return 0;
}

static int DeviceProductString(IHS_HIDDevice *device, IHS_Buffer *dest) {
    char *src = "Product String";
    IHS_BufferWriteMem(dest, 0, (unsigned char *) src, strlen(src) + 1);
    return 0;
}

static int DeviceSerialNumber(IHS_HIDDevice *device, IHS_Buffer *dest) {
    char *src = "0123456789abcdef";
    IHS_BufferWriteMem(dest, 0, (unsigned char *) src, strlen(src) + 1);
    return 0;
}