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

#include "hid/device.h"

#include "protobuf/pb_utils.h"
#include "session/channels/control/hid/manager.h"

#include <stdlib.h>

static void InfoFromHID(CHIDDeviceInfo *info, const IHS_StreamHIDDeviceInfo *hid);

static void SendRequestResponse(IHS_SessionChannel *channel, CHIDMessageFromRemote__RequestResponse *response);

void IHS_SessionChannelControlOnHIDMsg(IHS_SessionChannel *channel, const CHIDMessageToRemote *message) {
    IHS_SessionLog(channel->session, IHS_LogLevelDebug, "HID", "Message type: %u", message->command_case);
    IHS_SessionHIDManager *manager = channel->session->hidManager;
    switch (message->command_case) {
        case CHIDMESSAGE_TO_REMOTE__COMMAND_DEVICE_OPEN: {
            CHIDMessageToRemote__DeviceOpen *cmd = message->device_open;
            IHS_HIDDevice *device = IHS_HIDDeviceManagerOpenDevice(manager, cmd->info->path);
            // Send device ID as result
            CHIDMessageFromRemote__RequestResponse response = CHIDMESSAGE_FROM_REMOTE__REQUEST_RESPONSE__INIT;
            PROTOBUF_C_SET_VALUE(response, request_id, message->request_id);
            PROTOBUF_C_SET_VALUE(response, result, device->id);
            SendRequestResponse(channel, &response);
            break;
        }
        case CHIDMESSAGE_TO_REMOTE__COMMAND_DEVICE_CLOSE: {
            CHIDMessageToRemote__DeviceClose *cmd = message->device_close;
            IHS_HIDDevice *device = IHS_HIDDeviceManagerFindDevice(manager, cmd->device);
            IHS_HIDDeviceManagerCloseDevice(manager, device);
            // TODO send response?
            break;
        }
        case CHIDMESSAGE_TO_REMOTE__COMMAND_DEVICE_WRITE: {
            CHIDMessageToRemote__DeviceWrite *cmd = message->device_write;
            IHS_HIDDevice *device = IHS_HIDDeviceManagerFindDevice(manager, cmd->device);
            IHS_HIDDeviceWrite(device, cmd->data.data, cmd->data.len);
            break;
        }
        case CHIDMESSAGE_TO_REMOTE__COMMAND_DEVICE_READ: {
            CHIDMessageToRemote__DeviceRead *cmd = message->device_read;
            IHS_HIDDevice *device = IHS_HIDDeviceManagerFindDevice(manager, cmd->device);
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
            IHS_HIDDevice *device = IHS_HIDDeviceManagerFindDevice(manager, cmd->device);
            IHS_HIDDeviceSendFeatureReport(device, cmd->data.data, cmd->data.len);
            break;
        }
        case CHIDMESSAGE_TO_REMOTE__COMMAND_DEVICE_GET_FEATURE_REPORT: {
            CHIDMessageToRemote__DeviceGetFeatureReport *cmd = message->device_get_feature_report;
            IHS_HIDDevice *device = IHS_HIDDeviceManagerFindDevice(manager, cmd->device);
            IHS_Buffer str = IHS_BUFFER_INIT(cmd->length, 255);
            int result = IHS_HIDDeviceGetFeatureReport(device, cmd->report_number.data, cmd->report_number.len,
                                                       &str, cmd->length);

            CHIDMessageFromRemote__RequestResponse response = CHIDMESSAGE_FROM_REMOTE__REQUEST_RESPONSE__INIT;
            PROTOBUF_C_SET_VALUE(response, request_id, message->request_id);
            PROTOBUF_C_SET_VALUE(response, result, result);
            response.has_data = true;
            response.data.data = IHS_BufferPointer(&str);
            response.data.len = str.size;
            SendRequestResponse(channel, &response);
            break;
        }
        case CHIDMESSAGE_TO_REMOTE__COMMAND_DEVICE_GET_VENDOR_STRING: {
            CHIDMessageToRemote__DeviceGetVendorString *cmd = message->device_get_vendor_string;
            IHS_HIDDevice *device = IHS_HIDDeviceManagerFindDevice(manager, cmd->device);
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
            IHS_HIDDevice *device = IHS_HIDDeviceManagerFindDevice(manager, cmd->device);
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
            IHS_HIDDevice *device = IHS_HIDDeviceManagerFindDevice(manager, cmd->device);
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
            IHS_HIDDevice *device = IHS_HIDDeviceManagerFindDevice(manager, cmd->device);
            IHS_HIDDeviceStartInputReports(device, cmd->length);
            // TODO send response
            break;
        }
        case CHIDMESSAGE_TO_REMOTE__COMMAND_DEVICE_REQUEST_FULL_REPORT: {
            CHIDMessageToRemote__DeviceRequestFullReport *cmd = message->device_request_full_report;
            IHS_HIDDevice *device = IHS_HIDDeviceManagerFindDevice(manager, cmd->device);
            IHS_HIDDeviceRequestFullReport(device);
            // TODO send response
            break;
        }
        case CHIDMESSAGE_TO_REMOTE__COMMAND_DEVICE_DISCONNECT: {
            CHIDMessageToRemote__DeviceDisconnect *cmd = message->device_disconnect;
            IHS_HIDDevice *device = IHS_HIDDeviceManagerFindDevice(manager, cmd->device);
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
    bool ret = IHS_SessionChannelControlSend(channel, k_EStreamControlRemoteHID, (const ProtobufCMessage *) message,
                                             IHS_PACKET_ID_NEXT);
    free(wrapped.data.data);
    return ret;
}

bool IHS_SessionHIDNotifyChange(IHS_Session *session) {
    const IHS_StreamHIDInterface *hid = session->callbacks.hid;
    if (hid == NULL) {
        return false;
    }
    void *hidContext = session->callbackContexts.hid;
    IHS_StreamHIDDeviceEnumeration *devicesEnumeration = hid->enumerate(0, 0, hidContext);
    int numOfDevices = hid->enumeration_length(devicesEnumeration, hidContext);

    CHIDMessageFromRemote hidMessage = CHIDMESSAGE_FROM_REMOTE__INIT;
    CHIDMessageFromRemote__UpdateDeviceList updateDeviceList = CHIDMESSAGE_FROM_REMOTE__UPDATE_DEVICE_LIST__INIT;
    CHIDDeviceInfo *devices = NULL, **devicesList = NULL;
    updateDeviceList.n_devices = numOfDevices;
    if (numOfDevices > 0) {
        devices = calloc(numOfDevices, sizeof(CHIDDeviceInfo));
        devicesList = calloc(numOfDevices, sizeof(CHIDDeviceInfo *));
        int i = 0;
        IHS_StreamHIDDeviceInfo hidInfo;
        for (IHS_StreamHIDDeviceEnumeration *cur = devicesEnumeration; cur != NULL;
             cur = hid->enumeration_next(devicesEnumeration, hidContext)) {
            memset(&hidInfo, 0, sizeof(hidInfo));
            hid->enumeration_getinfo(devicesEnumeration, &hidInfo, hidContext);
            InfoFromHID(&devices[i], &hidInfo);
            devicesList[i] = &devices[i];
            i++;
        }
        updateDeviceList.devices = devicesList;
    }
    hid->free_enumeration(devicesEnumeration, hidContext);

    hidMessage.command_case = CHIDMESSAGE_FROM_REMOTE__COMMAND_UPDATE_DEVICE_LIST;
    hidMessage.update_device_list = &updateDeviceList;
    IHS_SessionChannel *channel = IHS_SessionChannelForType(session, IHS_SessionChannelTypeControl);
    bool ret = IHS_SessionChannelControlSendHIDMsg(channel, &hidMessage);
    if (devicesList != NULL) {
        free(devicesList);
    }
    if (devices != NULL) {
        free(devices);
    }
    return ret;
}


static void SendRequestResponse(IHS_SessionChannel *channel, CHIDMessageFromRemote__RequestResponse *response) {
    CHIDMessageFromRemote outMessage = CHIDMESSAGE_FROM_REMOTE__INIT;
    outMessage.command_case = CHIDMESSAGE_FROM_REMOTE__COMMAND_RESPONSE;
    outMessage.response = response;
    IHS_SessionChannelControlSendHIDMsg(channel, &outMessage);
}

static void InfoFromHID(CHIDDeviceInfo *info, const IHS_StreamHIDDeviceInfo *hid) {
    chiddevice_info__init(info);
    PROTOBUF_C_P_SET_VALUE(info, location, k_EDeviceLocationRemote);
    if (hid->vendor_id && hid->product_id) {
        PROTOBUF_C_P_SET_VALUE(info, vendor_id, hid->vendor_id);
        PROTOBUF_C_P_SET_VALUE(info, product_id, hid->product_id);
    }
}