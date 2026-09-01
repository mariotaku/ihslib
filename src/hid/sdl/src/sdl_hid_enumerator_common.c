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

#include "sdl_hid_enumerators.h"


/**
 * Everything this provider enumerates is an SDL_GameController — SDL's own mapping guarantees the
 * face buttons, d-pad and both sticks — so it reports the same superset for every device, which is
 * what ihslib announced unconditionally before providers could speak for themselves. The four bits
 * above the gamepad basics are the ones the reference sets for HIDAPI-backed and PS4/PS5 pads
 * (CHIDDeviceListSDL @ 0x1551bc); they are kept here because dropping them would change what the
 * host is told about existing devices, which is a separate question from this plumbing.
 */
#define SDL_GAMEPAD_CAPS (IHS_HID_CAP_ABXY | IHS_HID_CAP_DPAD | IHS_HID_CAP_LSTICK | IHS_HID_CAP_RSTICK | \
                          IHS_HID_CAP_MISC_1 | IHS_HID_CAP_XINPUT_OR_HIDAPI | IHS_HID_CAP_UNK_3 | \
                          IHS_HID_CAP_UNK_4 | IHS_HID_CAPS_ALWAYS)

bool IHS_HIDDeviceSDLEnumerationGetInfo(IHS_Enumeration *enumeration, IHS_HIDDeviceInfo *info) {
    const IHS_HIDDeviceSDLEnumerationClass *cls = (const IHS_HIDDeviceSDLEnumerationClass *) enumeration->cls;
    if (!cls->getInfo(enumeration, info)) {
        return false;
    }
    info->caps = SDL_GAMEPAD_CAPS;
    return true;
}
