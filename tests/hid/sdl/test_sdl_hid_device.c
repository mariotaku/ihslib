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
#include <SDL2/SDL.h>
#include <assert.h>

#include "ihslib/hid.h"
#include "ihslib/hid/sdl.h"

#include "ihs_enumeration.h"

#include "hid/manager.h"
#include "hid/provider.h"
#include "hid/device.h"
#include "hid/sdl/sdl_hid_common.h"

#include "test_session.h"
#include "ihslib/session.h"

int main(int argc, char *argv[]) {
    (void) argc;
    (void) argv;

    IHS_Init();

    SDL_Init(SDL_INIT_GAMECONTROLLER);
    if (SDL_NumJoysticks() > 0) {
        SDL_Quit();
        return 0;
    }
    int indices[8];
    indices[0] = SDL_JoystickAttachVirtual(SDL_JOYSTICK_TYPE_GAMECONTROLLER, 6, 16, 0);
    indices[1] = SDL_JoystickAttachVirtual(SDL_JOYSTICK_TYPE_WHEEL, 4, 8, 0);
    indices[2] = SDL_JoystickAttachVirtual(SDL_JOYSTICK_TYPE_GAMECONTROLLER, 6, 16, 0);
    indices[3] = SDL_JoystickAttachVirtual(SDL_JOYSTICK_TYPE_UNKNOWN, 8, 8, 0);
    indices[4] = SDL_JoystickAttachVirtual(SDL_JOYSTICK_TYPE_UNKNOWN, 8, 8, 0);
    indices[5] = SDL_JoystickAttachVirtual(SDL_JOYSTICK_TYPE_GAMECONTROLLER, 6, 16, 0);
    indices[6] = SDL_JoystickAttachVirtual(SDL_JOYSTICK_TYPE_DRUM_KIT, 0, 16, 0);
    indices[7] = SDL_JoystickAttachVirtual(SDL_JOYSTICK_TYPE_GAMECONTROLLER, 6, 16, 0);

    assert(SDL_NumJoysticks() == 8);

    IHS_Session *session = IHS_TestSessionCreate();
    IHS_HIDManager *manager = session->hidManager;
    IHS_HIDProvider *provider = IHS_HIDProviderSDLCreate(true);
    IHS_HIDManagerAddProvider(manager, provider);

    IHS_Enumeration *enumeration = IHS_HIDProviderEnumerateDevices(provider);
    assert(IHS_EnumerationCount(enumeration) == 4);
    for (IHS_EnumerationReset(enumeration); !IHS_EnumerationEnded(enumeration); IHS_EnumerationNext(enumeration)) {
        IHS_HIDDeviceInfo info;
        IHS_HIDProviderDeviceInfo(provider, enumeration, &info);
        printf("Device: %s\n", info.path);
    }
    assert(IHS_EnumerationNext(enumeration) == NULL);
    IHS_EnumerationFree(enumeration);

    assert(IHS_HIDManagerOpenDevice(manager, "bad://1") == NULL);
    assert(IHS_HIDManagerOpenDevice(manager, "sdl://9999") == NULL);
    assert(IHS_HIDManagerOpenDevice(manager, "sdl://aaaa") == NULL);

    SDL_JoystickID id = SDL_JoystickGetDeviceInstanceID(0);

    char path[16];
    snprintf(path, 16, "sdl://%d", id);
    IHS_HIDManagedDevice *managed = IHS_HIDManagerOpenDevice(manager, path);
    IHS_HIDDevice *device = managed->device;
    assert(device != NULL);

    assert(IHS_HIDManagerDeviceByJoystickID(manager, id) == managed);
    assert(IHS_HIDManagerDeviceByJoystickID(manager, 99999) == NULL);

    const static uint8_t maxRumble[21] = {0x1, 0xff, 0xff, 0xff, 0xff, 0x88, 0x13,};
    IHS_HIDDeviceWrite(device, maxRumble, 21);

    const static uint8_t setPlayerIndexTo7[21] = {0xb, 0x7,};
    IHS_HIDDeviceWrite(device, setPlayerIndexTo7, 21);
    assert(SDL_JoystickGetDevicePlayerIndex(0) == 7);

    const static uint8_t setLedRed[21] = {0x5, 0xff, 0x0, 0x0,};
    IHS_HIDDeviceWrite(device, setLedRed, 21);

    const static uint8_t unrecognized[21] = {0xff,};
    assert(IHS_HIDDeviceWrite(device, unrecognized, 21) == -1);

    const static uint8_t shortWrite[8] = {0x0,};
    assert(IHS_HIDDeviceWrite(device, shortWrite, 8) == -1);

    IHS_Buffer buffer = IHS_BUFFER_INIT(256, 256);

    const static uint8_t getDeviceReport[21] = {0x04};
    IHS_HIDDeviceGetFeatureReport(device, getDeviceReport, 21, &buffer, 65);

    const uint8_t *report = IHS_BufferPointerAt(&buffer, 1);
    assert(report[0] == true);
    assert(report[1] == false);

    IHS_HIDDeviceRead(device, &buffer, 30, 10);
    const uint8_t sendFeatureReport[21] = {0};
    assert(IHS_HIDDeviceSendFeatureReport(device, sendFeatureReport, 21) == -1);

    IHS_HIDDeviceGetVendorString(device, &buffer);
    IHS_HIDDeviceGetProductString(device, &buffer);
    IHS_HIDDeviceGetSerialNumberString(device, &buffer);

    IHS_BufferClear(&buffer, true);

    IHS_HIDManagedDeviceClose(managed);

    IHS_HIDProviderSDLDestroy(provider);

    IHS_SessionDestroy(session);

    SDL_Quit();

    IHS_Quit();
    return 0;
}
