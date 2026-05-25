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

// Regression test for the manager devices-list race: previously the array stored embedded
// IHS_HIDManagedDevice structs, and concurrent Open/Close racing against Find* would
// reallocate or shift the backing buffer out from under the reader. Now devices is a list
// of pointers to heap-allocated managed structs, guarded by manager->devicesLock, with
// deferred-free so a Find* result stays valid even if Close marks it dead.
//
// We don't directly assert on internal state — the value comes from running this under
// TSan in CI and Valgrind locally and observing zero races/UAFs/leaks. The asserts here
// just verify the API hasn't regressed (Find finds open, doesn't find closed).

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>

#include "hid/manager.h"
#include "hid/provider.h"
#include "hid/device.h"
#include "ihs_enumeration.h"
#include "ihs_thread.h"

#include "test_session.h"

#define ITERATIONS 2000

static IHS_HIDDevice *DeviceAlloc(const struct IHS_HIDDeviceClass *cls);
static void DeviceFree(IHS_HIDDevice *device);
static void DeviceClose(IHS_HIDDevice *device);
static int DeviceWrite(IHS_HIDDevice *device, const uint8_t *data, size_t dataLen);
static int DeviceRead(IHS_HIDDevice *device, IHS_Buffer *dest, size_t length, uint32_t timeoutMs);

static const IHS_HIDDeviceClass DeviceClass = {
        .alloc = DeviceAlloc,
        .free = DeviceFree,
        .close = DeviceClose,
        .write = DeviceWrite,
        .read = DeviceRead,
};

static bool ProviderSupportsDevice(IHS_HIDProvider *provider, const char *path) {
    (void) provider;
    return strstr(path, "test://") == path;
}

static IHS_HIDDevice *ProviderOpenDevice(IHS_HIDProvider *provider, const char *path) {
    (void) provider;
    (void) path;
    return IHS_HIDDeviceCreate(&DeviceClass);
}

static IHS_Enumeration *ProviderEnumerate(IHS_HIDProvider *provider) {
    (void) provider;
    static int empty;
    return IHS_EnumerationArrayCreate(&empty, sizeof(int), 0, NULL);
}

static IHS_HIDProvider *ProviderAlloc(const IHS_HIDProviderClass *cls) {
    IHS_HIDProvider *provider = calloc(1, sizeof(IHS_HIDProvider));
    provider->cls = cls;
    return provider;
}

static void ProviderFree(IHS_HIDProvider *provider) {
    free(provider);
}

static const IHS_HIDProviderClass ProviderClass = {
        .alloc = ProviderAlloc,
        .free = ProviderFree,
        .supportsDevice = ProviderSupportsDevice,
        .openDevice = ProviderOpenDevice,
        .enumerateDevices = ProviderEnumerate,
};

typedef struct {
    IHS_HIDManager *manager;
    atomic_uint findCalls;
    atomic_uint findHits;
} SharedState;

static void OpenCloseWorker(void *context) {
    SharedState *state = context;
    for (int i = 0; i < ITERATIONS; ++i) {
        IHS_HIDManagedDevice *managed = IHS_HIDManagerOpenDevice(state->manager, "test://x");
        if (managed != NULL) {
            IHS_HIDManagedDeviceClose(managed);
        }
    }
}

static void FindWorker(void *context) {
    SharedState *state = context;
    // Span the same ID range the producer is allocating into. Find returns the slot if
    // currently open, NULL if not-yet-allocated or already-closed. Either outcome
    // exercises the list-walk under devicesLock racing the producer's append/mark-closed.
    for (int i = 0; i < ITERATIONS; ++i) {
        uint32_t id = (uint32_t) ((i % ITERATIONS) + 1);
        atomic_fetch_add(&state->findCalls, 1);
        IHS_HIDManagedDevice *managed = IHS_HIDManagerFindDeviceByID(state->manager, id);
        if (managed != NULL) {
            // Don't assert !closed — that's the race the deferred-free model intentionally
            // tolerates: Find released devicesLock before returning, and another thread
            // may have flipped `closed` between then and now. The slot itself is still
            // valid memory (that's the guarantee), so reading these two fields is safe;
            // the id is immutable for the slot's lifetime.
            assert(managed->id == id);
            atomic_fetch_add(&state->findHits, 1);
        }
    }
}

int main() {
    IHS_Init();
    IHS_Session *session = IHS_TestSessionCreate();
    IHS_HIDManager *manager = session->hidManager;
    IHS_HIDProvider *provider = IHS_SessionHIDProviderCreate(&ProviderClass);
    IHS_HIDManagerAddProvider(manager, provider);

    // Sanity: open, find, close, find-returns-NULL.
    IHS_HIDManagedDevice *m = IHS_HIDManagerOpenDevice(manager, "test://0");
    assert(m != NULL);
    uint32_t baselineId = m->id;
    assert(IHS_HIDManagerFindDeviceByID(manager, baselineId) == m);
    IHS_HIDManagedDeviceClose(m);
    // After close, the slot stays alive but Find skips it.
    assert(IHS_HIDManagerFindDeviceByID(manager, baselineId) == NULL);

    // Concurrent open/close vs find. Pre-TSan/this-fix, the find walk would race the
    // append's potential realloc of devices.data and/or read shifted memory after a
    // remove. Now Find takes devicesLock and slots never shift.
    SharedState state = {.manager = manager};
    atomic_init(&state.findCalls, 0);
    atomic_init(&state.findHits, 0);
    IHS_Thread *t1 = IHS_ThreadCreate(OpenCloseWorker, "ihs-open-close", &state);
    IHS_Thread *t2 = IHS_ThreadCreate(FindWorker, "ihs-find", &state);
    IHS_ThreadJoin(t1);
    IHS_ThreadJoin(t2);

    // Both threads ran a fixed iteration count, so findCalls is exactly ITERATIONS.
    // The hit count varies wildly with scheduling — just confirm the loop executed.
    assert(atomic_load(&state.findCalls) == ITERATIONS);

    IHS_HIDManagerRemoveProvider(manager, provider);
    IHS_SessionHIDProviderDestroy(provider);
    IHS_SessionDestroy(session);
    IHS_Quit();
    return 0;
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
    (void) device;
}

static int DeviceWrite(IHS_HIDDevice *device, const uint8_t *data, size_t dataLen) {
    (void) device;
    (void) data;
    (void) dataLen;
    return 0;
}

static int DeviceRead(IHS_HIDDevice *device, IHS_Buffer *dest, size_t length, uint32_t timeoutMs) {
    (void) device;
    (void) dest;
    (void) length;
    (void) timeoutMs;
    return 0;
}
