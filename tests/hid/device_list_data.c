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
#include <stdbool.h>
#include <string.h>

#include "protobuf/hiddevices.pb-c.h"
#include "protobuf/pb_utils.h"
#include "ihslib/hid.h"

static void WriteDeviceInfo(CHIDDeviceInfo *info) {
    chiddevice_info__init(info);
    PROTOBUF_C_P_SET_VALUE(info, location, k_EDeviceLocationLocal);
    info->path = "sdl://0";
    PROTOBUF_C_P_SET_VALUE(info, vendor_id, 1118);
    PROTOBUF_C_P_SET_VALUE(info, product_id, 746);
    PROTOBUF_C_P_SET_VALUE(info, release_number, 1293);
    PROTOBUF_C_P_SET_VALUE(info, usage_page, 1);
    PROTOBUF_C_P_SET_VALUE(info, usage, 5/*For SDL_GameController*/);
    info->product_string = "SDL Gamepad";
    PROTOBUF_C_P_SET_VALUE(info, is_generic_gamepad, true);
    /*Seems to be corresponding to kernel version. See stream_client/GetOSType */
    PROTOBUF_C_P_SET_VALUE(info, ostype, -203);

    IHS_HIDDeviceCaps capsBits = IHS_HID_CAP_ABXY | IHS_HID_CAP_DPAD | IHS_HID_CAP_LSTICK | IHS_HID_CAP_RSTICK |
                                 IHS_HID_CAP_STICKBTNS | IHS_HID_CAP_SHOULDERS | IHS_HID_CAP_TRIGGERS |
                                 IHS_HID_CAP_BACK | IHS_HID_CAP_START | IHS_HID_CAP_GUIDE |
                                 IHS_HID_CAP_NOT_XINPUT_NOT_HIDAPI;
    PROTOBUF_C_P_SET_VALUE(info, caps_bits, capsBits);
}

static void TestInfoItem() {
    CHIDDeviceInfo info;
    WriteDeviceInfo(&info);
    assert(chiddevice_info__get_packed_size(&info) == 54);
    uint8_t info_bytes[54];
    static const uint8_t info_expected[54] = {
            0x08, 0x00, 0x12, 0x07, 0x73, 0x64, 0x6c, 0x3a,
            0x2f, 0x2f, 0x30, 0x18, 0xde, 0x08, 0x20, 0xea,
            0x05, 0x30, 0x8d, 0x0a, 0x42, 0x0b, 0x53, 0x44,
            0x4c, 0x20, 0x47, 0x61, 0x6d, 0x65, 0x70, 0x61,
            0x64, 0x48, 0x01, 0x50, 0x05, 0x60, 0xb5, 0xfe,
            0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x01,
            0x68, 0x01, 0x78, 0xff, 0x87, 0x04};
    chiddevice_info__pack(&info, info_bytes);
    assert(memcmp(info_bytes, info_expected, 54) == 0);

}

static void TestFromRemoteMsg() {
    CHIDMessageFromRemote msg = CHIDMESSAGE_FROM_REMOTE__INIT;
    msg.command_case = CHIDMESSAGE_FROM_REMOTE__COMMAND_UPDATE_DEVICE_LIST;

    CHIDMessageFromRemote__UpdateDeviceList list = CHIDMESSAGE_FROM_REMOTE__UPDATE_DEVICE_LIST__INIT;
    msg.update_device_list = &list;

    CHIDDeviceInfo info;
    WriteDeviceInfo(&info);

    CHIDDeviceInfo *devices[1];
    devices[0] = &info;

    list.n_devices = 1;
    list.devices = devices;

    assert(chidmessage_from_remote__get_packed_size(&msg) == 58);

    static const uint8_t msg_expected[58] = {
            0x0a, 0x38, 0x0a, 0x36, 0x08, 0x00, 0x12, 0x07,
            0x73, 0x64, 0x6c, 0x3a, 0x2f, 0x2f, 0x30, 0x18,
            0xde, 0x08, 0x20, 0xea, 0x05, 0x30, 0x8d, 0x0a,
            0x42, 0x0b, 0x53, 0x44, 0x4c, 0x20, 0x47, 0x61,
            0x6d, 0x65, 0x70, 0x61, 0x64, 0x48, 0x01, 0x50,
            0x05, 0x60, 0xb5, 0xfe, 0xff, 0xff, 0xff, 0xff,
            0xff, 0xff, 0xff, 0x01, 0x68, 0x01, 0x78, 0xff,
            0x87, 0x04
    };

    uint8_t msg_bytes[58];
    chidmessage_from_remote__pack(&msg, msg_bytes);
    assert(memcmp(msg_bytes, msg_expected, 58) == 0);
}

int main() {
    TestInfoItem();
    TestFromRemoteMsg();
    return 0;
}
