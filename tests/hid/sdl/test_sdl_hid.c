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

#include "ihs_enumeration.h"

#include "hid/manager.h"
#include "hid/provider.h"
#include "hid/device.h"

int main(int argc, char *argv[]) {
    SDL_Init(SDL_INIT_GAMECONTROLLER);
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

    IHS_HIDManager *manager = IHS_HIDManagerCreate();
    IHS_HIDProvider *provider = IHS_HIDProviderSDLCreate();
    IHS_HIDManagerAddProvider(manager, provider);

    IHS_Enumeration *enumeration = IHS_HIDProviderEnumerateDevices(provider);
    assert(IHS_EnumerationCount(enumeration) == 4);
    for (IHS_EnumerationReset(enumeration); !IHS_EnumerationEnded(enumeration); IHS_EnumerationNext(enumeration)) {
        IHS_StreamHIDDeviceInfo info;
        IHS_HIDProviderDeviceInfo(provider, enumeration, &info);
        printf("Device: %s\n", info.path);
    }
    assert(IHS_EnumerationNext(enumeration) == NULL);
    assert(IHS_HIDManagerOpenDevice(manager, "bad://1") == NULL);
    assert(IHS_HIDManagerOpenDevice(manager, "sdl://1") == NULL);
    assert(IHS_HIDManagerOpenDevice(manager, "sdl://aaaa") == NULL);

    IHS_HIDDevice *device = IHS_HIDManagerOpenDevice(manager, "sdl://0");
    assert(device != NULL);
    IHS_HIDDeviceClose(device);

    IHS_HIDProviderSDLDestroy(provider);

    IHS_HIDManagerDestroy(manager);

    SDL_Quit();
    return 0;
}