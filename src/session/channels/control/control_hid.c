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
#include "control_hid.h"
#include "session/channels/ch_control.h"
#include "session/session_pri.h"

#include "protobuf/pb_utils.h"

#include "hid/device.h"
#include "hid/manager.h"
#include "hid/provider.h"

#include "ihs_enumeration.h"

#include <stdlib.h>

const static char *const ToRemoteCommandNames[CHIDMESSAGE_TO_REMOTE__COMMAND_DEVICE_DISCONNECT + 1] = {
        "Not set",
        "",
        "DeviceOpen",
        "DeviceClose",
        "DeviceWrite",
        "DeviceRead",
        "DeviceSendFeatureReport",
        "DeviceGetFeatureReport",
        "DeviceGetVendorString",
        "DeviceGetProductString",
        "DeviceGetSerialNumberString",
        "DeviceStartInputReports",
        "DeviceRequestFullReport",
        "DeviceDisconnect",
};

static void InfoFromHID(CHIDDeviceInfo *info, const IHS_HIDDeviceInfo *hid);

static void SendRequestResponse(IHS_SessionChannel *channel, CHIDMessageFromRemote__RequestResponse *response);

void IHS_SessionChannelControlOnHIDMsg(IHS_SessionChannel *channel, const CHIDMessageToRemote *message) {
    IHS_SessionLog(channel->session, IHS_LogLevelDebug, "HID", "Message type: %s",
                   ToRemoteCommandNames[message->command_case]);
    IHS_HIDManager *manager = channel->session->hidManager;
    switch (message->command_case) {
        case CHIDMESSAGE_TO_REMOTE__COMMAND_DEVICE_OPEN: {
            CHIDMessageToRemote__DeviceOpen *cmd = message->device_open;
            IHS_HIDManagedDevice *managed = IHS_HIDManagerOpenDevice(manager, cmd->info->path);
            // Send managed ID as result
            CHIDMessageFromRemote__RequestResponse response = CHIDMESSAGE_FROM_REMOTE__REQUEST_RESPONSE__INIT;
            PROTOBUF_C_SET_VALUE(response, request_id, message->request_id);
            PROTOBUF_C_SET_VALUE(response, result, managed->id);
            SendRequestResponse(channel, &response);
            break;
        }
        case CHIDMESSAGE_TO_REMOTE__COMMAND_DEVICE_CLOSE: {
            CHIDMessageToRemote__DeviceClose *cmd = message->device_close;
            IHS_HIDDevice *device = (IHS_HIDDevice *) IHS_HIDManagerFindDeviceByID(manager, cmd->device);
            IHS_HIDDeviceClose(device);
            // TODO send response?
            break;
        }
        case CHIDMESSAGE_TO_REMOTE__COMMAND_DEVICE_WRITE: {
            CHIDMessageToRemote__DeviceWrite *cmd = message->device_write;
            IHS_HIDManagedDevice *managed = IHS_HIDManagerFindDeviceByID(manager, cmd->device);
            IHS_HIDDeviceWrite(managed->device, cmd->data.data, cmd->data.len);
            break;
        }
        case CHIDMESSAGE_TO_REMOTE__COMMAND_DEVICE_READ: {
            CHIDMessageToRemote__DeviceRead *cmd = message->device_read;
            IHS_HIDDevice *device = (IHS_HIDDevice *) IHS_HIDManagerFindDeviceByID(manager, cmd->device);
            IHS_Buffer str = IHS_BUFFER_INIT(cmd->length, 255);
            int result = IHS_HIDDeviceRead(device, &str, cmd->length, cmd->timeout_ms);

            CHIDMessageFromRemote__RequestResponse response = CHIDMESSAGE_FROM_REMOTE__REQUEST_RESPONSE__INIT;
            PROTOBUF_C_SET_VALUE(response, request_id, message->request_id);
            PROTOBUF_C_SET_VALUE(response, result, result);
            response.has_data = true;
            response.data.data = IHS_BufferPointer(&str);
            response.data.len = str.size;
            SendRequestResponse(channel, &response);
            break;
        }
        case CHIDMESSAGE_TO_REMOTE__COMMAND_DEVICE_SEND_FEATURE_REPORT: {
            CHIDMessageToRemote__DeviceSendFeatureReport *cmd = message->device_send_feature_report;
            IHS_HIDDevice *device = (IHS_HIDDevice *) IHS_HIDManagerFindDeviceByID(manager, cmd->device);
            IHS_HIDDeviceSendFeatureReport(device, cmd->data.data, cmd->data.len);
            break;
        }
        case CHIDMESSAGE_TO_REMOTE__COMMAND_DEVICE_GET_FEATURE_REPORT: {
            CHIDMessageToRemote__DeviceGetFeatureReport *cmd = message->device_get_feature_report;
            IHS_HIDDevice *device = (IHS_HIDDevice *) IHS_HIDManagerFindDeviceByID(manager, cmd->device);
            IHS_Buffer str = IHS_BUFFER_INIT(cmd->length, 255);
            int result = IHS_HIDDeviceGetFeatureReport(device, cmd->report_number.data, cmd->report_number.len,
                                                       &str, cmd->length);

            CHIDMessageFromRemote__RequestResponse response = CHIDMESSAGE_FROM_REMOTE__REQUEST_RESPONSE__INIT;
            PROTOBUF_C_SET_VALUE(response, request_id, message->request_id);
            PROTOBUF_C_SET_VALUE(response, result, result);
            if (result == 0) {
                response.has_data = true;
                response.data.data = IHS_BufferPointer(&str);
                response.data.len = str.size;
            }
            SendRequestResponse(channel, &response);
            IHS_BufferClear(&str, true);
            break;
        }
        case CHIDMESSAGE_TO_REMOTE__COMMAND_DEVICE_GET_VENDOR_STRING: {
            CHIDMessageToRemote__DeviceGetVendorString *cmd = message->device_get_vendor_string;
            IHS_HIDDevice *device = (IHS_HIDDevice *) IHS_HIDManagerFindDeviceByID(manager, cmd->device);
            IHS_Buffer str = IHS_BUFFER_INIT(0, 255);
            int result = IHS_HIDDeviceGetVendorString(device, &str);

            CHIDMessageFromRemote__RequestResponse response = CHIDMESSAGE_FROM_REMOTE__REQUEST_RESPONSE__INIT;
            PROTOBUF_C_SET_VALUE(response, request_id, message->request_id);
            PROTOBUF_C_SET_VALUE(response, result, result);
            response.has_data = true;
            response.data.data = IHS_BufferPointer(&str);
            response.data.len = str.size;
            SendRequestResponse(channel, &response);
            break;
        }
        case CHIDMESSAGE_TO_REMOTE__COMMAND_DEVICE_GET_PRODUCT_STRING: {
            CHIDMessageToRemote__DeviceGetProductString *cmd = message->device_get_product_string;
            IHS_HIDDevice *device = (IHS_HIDDevice *) IHS_HIDManagerFindDeviceByID(manager, cmd->device);
            IHS_Buffer str = IHS_BUFFER_INIT(0, 255);
            int result = IHS_HIDDeviceGetProductString(device, &str);

            CHIDMessageFromRemote__RequestResponse response = CHIDMESSAGE_FROM_REMOTE__REQUEST_RESPONSE__INIT;
            PROTOBUF_C_SET_VALUE(response, request_id, message->request_id);
            PROTOBUF_C_SET_VALUE(response, result, result);
            response.has_data = true;
            response.data.data = IHS_BufferPointer(&str);
            response.data.len = str.size;
            SendRequestResponse(channel, &response);
            break;
        }
        case CHIDMESSAGE_TO_REMOTE__COMMAND_DEVICE_GET_SERIAL_NUMBER_STRING: {
            CHIDMessageToRemote__DeviceGetSerialNumberString *cmd = message->device_get_serial_number_string;
            IHS_HIDDevice *device = (IHS_HIDDevice *) IHS_HIDManagerFindDeviceByID(manager, cmd->device);
            IHS_Buffer str = IHS_BUFFER_INIT(0, 255);
            int result = IHS_HIDDeviceGetSerialNumberString(device, &str);

            CHIDMessageFromRemote__RequestResponse response = CHIDMESSAGE_FROM_REMOTE__REQUEST_RESPONSE__INIT;
            PROTOBUF_C_SET_VALUE(response, request_id, message->request_id);
            PROTOBUF_C_SET_VALUE(response, result, result);
            response.has_data = true;
            response.data.data = IHS_BufferPointer(&str);
            response.data.len = str.size;
            SendRequestResponse(channel, &response);
            break;
        }
        case CHIDMESSAGE_TO_REMOTE__COMMAND_DEVICE_START_INPUT_REPORTS: {
            CHIDMessageToRemote__DeviceStartInputReports *cmd = message->device_start_input_reports;
            IHS_HIDManagedDevice *device = IHS_HIDManagerFindDeviceByID(manager, cmd->device);
            IHS_HIDReportHolderSetReportLength(&device->reportHolder, cmd->length);
            IHS_HIDDeviceStartInputReports(device->device, cmd->length);
            // TODO send response
            break;
        }
        case CHIDMESSAGE_TO_REMOTE__COMMAND_DEVICE_REQUEST_FULL_REPORT: {
            CHIDMessageToRemote__DeviceRequestFullReport *cmd = message->device_request_full_report;
            IHS_HIDDevice *device = (IHS_HIDDevice *) IHS_HIDManagerFindDeviceByID(manager, cmd->device);
            IHS_HIDDeviceRequestFullReport(device);
            // TODO send response
            break;
        }
        case CHIDMESSAGE_TO_REMOTE__COMMAND_DEVICE_DISCONNECT: {
            CHIDMessageToRemote__DeviceDisconnect *cmd = message->device_disconnect;
            IHS_HIDDevice *device = (IHS_HIDDevice *) IHS_HIDManagerFindDeviceByID(manager, cmd->device);
            IHS_HIDDeviceRequestDisconnect(device, cmd->disconnectmethod, cmd->data.data, cmd->data.len);
            // TODO send response
            break;
        }
        default:
            break;
    }
}

bool IHS_SessionChannelControlSendHIDMsg(IHS_SessionChannel *channel, const CHIDMessageFromRemote *message) {
    CRemoteHIDMsg wrapped = CREMOTE_HIDMSG__INIT;
    size_t messageSize = chidmessage_from_remote__get_packed_size(message);
    wrapped.has_data = true;
    wrapped.data.data = malloc(messageSize);
    wrapped.data.len = messageSize;
    chidmessage_from_remote__pack(message, wrapped.data.data);
    bool ret = IHS_SessionChannelControlSend(channel, k_EStreamControlRemoteHID, (const ProtobufCMessage *) &wrapped,
                                             IHS_PACKET_ID_NEXT);
    free(wrapped.data.data);
    return ret;
}

bool IHS_SessionHIDNotifyDeviceChange(IHS_Session *session) {
    IHS_HIDManager *manager = session->hidManager;
    CHIDMessageFromRemote hidMessage = CHIDMESSAGE_FROM_REMOTE__INIT;
    CHIDMessageFromRemote__UpdateDeviceList updateDeviceList = CHIDMESSAGE_FROM_REMOTE__UPDATE_DEVICE_LIST__INIT;
    CHIDDeviceInfo *allDevices = NULL, **allDevicePointers = NULL;
    int numAllDevices = 0;

    IHS_SessionLog(session, IHS_LogLevelDebug, "HID", "Start enumerate device. Number of providers: %u",
                   manager->providers.size);

    for (int pi = 0, ps = (int) manager->providers.size; pi < ps; pi++) {
        IHS_HIDProvider *provider = *((IHS_HIDProvider **) IHS_ArrayListGet(&manager->providers, pi));
        IHS_Enumeration *e = IHS_HIDProviderEnumerateDevices(provider);
        size_t numDevices = IHS_EnumerationCount(e);
        if (numDevices != 0) {
            allDevices = realloc(allDevices, (numAllDevices + numDevices) * sizeof(CHIDDeviceInfo));
            for (IHS_EnumerationReset(e); !IHS_EnumerationEnded(e); IHS_EnumerationNext(e)) {
                IHS_HIDDeviceInfo hid;
                IHS_HIDProviderDeviceInfo(provider, e, &hid);
                InfoFromHID(&allDevices[numAllDevices], &hid);
                IHS_SessionLog(session, IHS_LogLevelDebug, "HID", "Device found: %s, vid=%u, pid=%u", hid.path,
                               hid.vendor_id, hid.product_id);
                numAllDevices++;
            }
        }
        IHS_EnumerationFree(e);
    }

    allDevicePointers = calloc(numAllDevices, sizeof(CHIDDeviceInfo *));
    for (int di = 0; di < numAllDevices; di++) {
        allDevicePointers[di] = &allDevices[di];
    }
    updateDeviceList.n_devices = numAllDevices;
    updateDeviceList.devices = allDevicePointers;

    hidMessage.command_case = CHIDMESSAGE_FROM_REMOTE__COMMAND_UPDATE_DEVICE_LIST;
    hidMessage.update_device_list = &updateDeviceList;
    IHS_SessionChannel *channel = IHS_SessionChannelForType(session, IHS_SessionChannelTypeControl);
    bool ret = IHS_SessionChannelControlSendHIDMsg(channel, &hidMessage);
    if (allDevicePointers != NULL) {
        free(allDevicePointers);
    }
    if (allDevices != NULL) {
        for (int di = 0; di < numAllDevices; di++) {
            free(allDevices[di].path);
        }
        free(allDevices);
    }
    return ret;
}

bool IHS_SessionHIDSendReport(IHS_Session *session) {
    CHIDMessageFromRemote outMessage = CHIDMESSAGE_FROM_REMOTE__INIT;
    outMessage.command_case = CHIDMESSAGE_FROM_REMOTE__COMMAND_REPORTS;
    CHIDMessageFromRemote__DeviceInputReports reports = CHIDMESSAGE_FROM_REMOTE__DEVICE_INPUT_REPORTS__INIT;
    outMessage.reports = &reports;

    // Lock all devices to prevent new reports
    for (size_t i = 0, j = session->hidManager->devices.size; i < j; ++i) {
        IHS_HIDManagedDevice *device = IHS_ArrayListGet(&session->hidManager->devices, i);
        IHS_MutexLock(device->lock);
    }

    IHS_ArrayListClear(&session->hidManager->inputReports);
    for (size_t i = 0, j = session->hidManager->devices.size; i < j; ++i) {
        IHS_HIDManagedDevice *device = IHS_ArrayListGet(&session->hidManager->devices, i);
        IHS_HIDDeviceReportMessage *report = IHS_HIDReportHolderGetMessage(&device->reportHolder, device->device);
        if (report == NULL) {
            continue;
        }
        IHS_ArrayListAppend(&session->hidManager->inputReports, &report);
    }

    bool ret = false;
    if (session->hidManager->inputReports.size > 0) {
        reports.n_device_reports = session->hidManager->inputReports.size;
        reports.device_reports = (IHS_HIDDeviceReportMessage **) session->hidManager->inputReports.data;

        IHS_SessionChannel *channel = IHS_SessionChannelForType(session, IHS_SessionChannelTypeControl);
        ret = IHS_SessionChannelControlSendHIDMsg(channel, &outMessage);
    }

    // Unlock all devices
    for (size_t i = 0, j = session->hidManager->devices.size; i < j; ++i) {
        IHS_HIDManagedDevice *device = IHS_ArrayListGet(&session->hidManager->devices, i);
        IHS_MutexUnlock(device->lock);
    }
    return ret;
}

static void SendRequestResponse(IHS_SessionChannel *channel, CHIDMessageFromRemote__RequestResponse *response) {
    CHIDMessageFromRemote outMessage = CHIDMESSAGE_FROM_REMOTE__INIT;
    outMessage.command_case = CHIDMESSAGE_FROM_REMOTE__COMMAND_RESPONSE;
    outMessage.response = response;
    IHS_SessionChannelControlSendHIDMsg(channel, &outMessage);
}

static void InfoFromHID(CHIDDeviceInfo *info, const IHS_HIDDeviceInfo *hid) {
    chiddevice_info__init(info);
    PROTOBUF_C_P_SET_VALUE(info, location, k_EDeviceLocationLocal);
    info->path = strdup(hid->path);
    if (hid->vendor_id && hid->product_id) {
        PROTOBUF_C_P_SET_VALUE(info, vendor_id, hid->vendor_id);
        PROTOBUF_C_P_SET_VALUE(info, product_id, hid->product_id);
        PROTOBUF_C_P_SET_VALUE(info, release_number, hid->product_version);
    }
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