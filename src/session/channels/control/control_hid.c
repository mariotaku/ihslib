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
#include "control_hid.h"
#include "session/channels/ch_control.h"
#include "session/session_pri.h"

#include "protobuf/pb_utils.h"

#include "hid/device.h"
#include "hid/manager.h"
#include "hid/provider.h"

#include "ihs_enumeration.h"

#include <stdlib.h>
#include <unistd.h>

static void HandleDeviceOpen(IHS_SessionChannel *channel, IHS_HIDManager *manager, const CHIDMessageToRemote *message);

static void HandleDeviceClose(IHS_SessionChannel *channel, IHS_HIDManager *manager, const CHIDMessageToRemote *message);

static void HandleDeviceWrite(IHS_SessionChannel *channel, IHS_HIDManager *manager, const CHIDMessageToRemote *message);

static void HandleDeviceRead(IHS_SessionChannel *channel, IHS_HIDManager *manager, const CHIDMessageToRemote *message);

static void HandleDeviceSendFeatureReport(IHS_SessionChannel *channel, IHS_HIDManager *manager,
                                          const CHIDMessageToRemote *message);

static void HandleDeviceGetFeatureReport(IHS_SessionChannel *channel, IHS_HIDManager *manager,
                                         const CHIDMessageToRemote *message);

static void HandleDeviceGetVendorString(IHS_SessionChannel *channel, IHS_HIDManager *manager,
                                        const CHIDMessageToRemote *message);

static void HandleDeviceGetProductString(IHS_SessionChannel *channel, IHS_HIDManager *manager,
                                         const CHIDMessageToRemote *message);

static void HandleDeviceGetSerialNumberString(IHS_SessionChannel *channel, IHS_HIDManager *manager,
                                              const CHIDMessageToRemote *message);

static void HandleDeviceStartInputReports(IHS_SessionChannel *channel, IHS_HIDManager *manager,
                                          const CHIDMessageToRemote *message);

static void HandleDeviceRequestFullReport(IHS_SessionChannel *channel, IHS_HIDManager *manager,
                                          const CHIDMessageToRemote *message);

static void HandleDeviceDisconnect(IHS_SessionChannel *channel, IHS_HIDManager *manager,
                                   const CHIDMessageToRemote *message);

static void InfoFromHID(CHIDDeviceInfo *info, const IHS_HIDDeviceInfo *hid);

static void SendRequestResponse(IHS_SessionChannel *channel, CHIDMessageFromRemote__RequestResponse *response);

static void SendRequestCodeResponse(IHS_SessionChannel *channel, uint32_t requestId, int result);

void IHS_SessionChannelControlOnHIDMsg(IHS_SessionChannel *channel, const CHIDMessageToRemote *message) {
    IHS_HIDManager *manager = channel->session->hidManager;
    switch (message->command_case) {
        case CHIDMESSAGE_TO_REMOTE__COMMAND_DEVICE_OPEN: {
            HandleDeviceOpen(channel, manager, message);
            break;
        }
        case CHIDMESSAGE_TO_REMOTE__COMMAND_DEVICE_CLOSE: {
            HandleDeviceClose(channel, manager, message);
            break;
        }
        case CHIDMESSAGE_TO_REMOTE__COMMAND_DEVICE_WRITE: {
            HandleDeviceWrite(channel, manager, message);
            break;
        }
        case CHIDMESSAGE_TO_REMOTE__COMMAND_DEVICE_READ: {
            HandleDeviceRead(channel, manager, message);
            break;
        }
        case CHIDMESSAGE_TO_REMOTE__COMMAND_DEVICE_SEND_FEATURE_REPORT: {
            HandleDeviceSendFeatureReport(channel, manager, message);
            break;
        }
        case CHIDMESSAGE_TO_REMOTE__COMMAND_DEVICE_GET_FEATURE_REPORT: {
            HandleDeviceGetFeatureReport(channel, manager, message);
            break;
        }
        case CHIDMESSAGE_TO_REMOTE__COMMAND_DEVICE_GET_VENDOR_STRING: {
            HandleDeviceGetVendorString(channel, manager, message);
            break;
        }
        case CHIDMESSAGE_TO_REMOTE__COMMAND_DEVICE_GET_PRODUCT_STRING: {
            HandleDeviceGetProductString(channel, manager, message);
            break;
        }
        case CHIDMESSAGE_TO_REMOTE__COMMAND_DEVICE_GET_SERIAL_NUMBER_STRING: {
            HandleDeviceGetSerialNumberString(channel, manager, message);
            break;
        }
        case CHIDMESSAGE_TO_REMOTE__COMMAND_DEVICE_START_INPUT_REPORTS: {
            HandleDeviceStartInputReports(channel, manager, message);
            break;
        }
        case CHIDMESSAGE_TO_REMOTE__COMMAND_DEVICE_REQUEST_FULL_REPORT: {
            HandleDeviceRequestFullReport(channel, manager, message);
            break;
        }
        case CHIDMESSAGE_TO_REMOTE__COMMAND_DEVICE_DISCONNECT: {
            HandleDeviceDisconnect(channel, manager, message);
            break;
        }
        default: {
            break;
        }
    }
}

bool IHS_SessionChannelControlSendHIDMsg(IHS_SessionChannel *channel, const CHIDMessageFromRemote *message) {
    // Mirror Steam's CStreamClient::SendRemoteHIDMessage (0x1fa1c8) which gates on
    // IsStreaming() && BStreamingInput(). If the server disabled input streaming via
    // OnSetClientConfig, drop the message client-side rather than burning bandwidth on
    // something the server has told us it isn't reading.
    if (!IHS_SessionInputEnabled(channel->session)) return false;
    CRemoteHIDMsg wrapped = CREMOTE_HIDMSG__INIT;
    size_t messageSize = chidmessage_from_remote__get_packed_size(message);
    wrapped.has_data = true;
    wrapped.data.data = malloc(messageSize);
    if (wrapped.data.data == NULL) {
        IHS_SessionLog(channel->session, IHS_LogLevelWarn, "HID",
                       "Failed to allocate %zu bytes for HID message", messageSize);
        return false;
    }
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
                IHS_HIDDeviceInfo hid = { NULL };
                IHS_HIDProviderDeviceInfo(provider, e, &hid);
                InfoFromHID(&allDevices[numAllDevices], &hid);
                IHS_SessionLog(session, IHS_LogLevelDebug, "HID", "Device found: %s, id=%04x:%04x, name=%s", hid.path,
                               hid.vendor_id, hid.product_id, hid.product_string);
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
            free(allDevices[di].product_string);
            free(allDevices[di].path);
        }
        free(allDevices);
    }
    return ret;
}

bool IHS_SessionHIDSendReport(IHS_Session *session) {
    // Short-circuit before the per-device lock storm if input streaming is disabled.
    // The polling task still calls device->poll() so device state stays current; only
    // the wire send is suppressed (matches Steam's BStreamingInput gating behavior).
    if (!IHS_SessionInputEnabled(session)) return false;
    CHIDMessageFromRemote outMessage = CHIDMESSAGE_FROM_REMOTE__INIT;
    outMessage.command_case = CHIDMESSAGE_FROM_REMOTE__COMMAND_REPORTS;
    CHIDMessageFromRemote__DeviceInputReports reports = CHIDMESSAGE_FROM_REMOTE__DEVICE_INPUT_REPORTS__INIT;
    outMessage.reports = &reports;

    // Snapshot the open device list once under devicesLock. The snapshot pointers stay
    // valid for the rest of this function: closed slots are kept alive in the manager,
    // and even devices that flip to closed mid-send still have a live struct (the inner
    // IHS_HIDDevice may be gone, but we only touch managed->lock and managed->reportHolder
    // below, both of which are owned by the outer struct).
    size_t deviceCount;
    IHS_HIDManagedDevice **deviceSnapshot = IHS_HIDManagerSnapshotOpenDevices(session->hidManager, &deviceCount);

    // Lock all snapshot devices to prevent new reports while we copy.
    for (size_t i = 0; i < deviceCount; ++i) {
        IHS_MutexLock(deviceSnapshot[i]->lock);
    }

    IHS_ArrayListClear(&session->hidManager->inputReports);
    for (size_t i = 0; i < deviceCount; ++i) {
        IHS_HIDDeviceReportMessage *report = IHS_HIDReportHolderGetMessage(&deviceSnapshot[i]->reportHolder);
        if (report == NULL) {
            continue;
        }
        IHS_ArrayListAppend(&session->hidManager->inputReports, &report);
    }

    // Pack CHIDMessageFromRemote into a flat byte buffer while the holders are still
    // locked — the per-device report messages have pointers into the holders' internal
    // dataBuffers. Once packed, the buffer is independent and we can release the locks
    // before doing the expensive encrypt + send work.
    uint8_t *packed = NULL;
    size_t packedLen = 0;
    if (session->hidManager->inputReports.size > 0) {
        reports.n_device_reports = session->hidManager->inputReports.size;
        reports.device_reports = (IHS_HIDDeviceReportMessage **) session->hidManager->inputReports.data;
        packedLen = chidmessage_from_remote__get_packed_size(&outMessage);
        packed = malloc(packedLen);
        if (packed != NULL) {
            chidmessage_from_remote__pack(&outMessage, packed);
        } else {
            IHS_SessionLog(session, IHS_LogLevelWarn, "HID",
                           "Failed to allocate %zu bytes for HID report", packedLen);
        }
    }

    // Reset holders and unlock — the packed snapshot is self-contained from here on.
    for (size_t i = 0; i < deviceCount; ++i) {
        IHS_HIDReportHolderResetMessage(&deviceSnapshot[i]->reportHolder);
        IHS_MutexUnlock(deviceSnapshot[i]->lock);
    }
    free(deviceSnapshot);

    bool ret = false;
    if (packed != NULL) {
        // Wrap the pre-packed CHIDMessageFromRemote in a CRemoteHIDMsg and ship it. The
        // crypto and send-queue work happens with zero device locks held; SDL events on
        // those devices can now proceed concurrently with the wire send.
        CRemoteHIDMsg wrapped = CREMOTE_HIDMSG__INIT;
        wrapped.has_data = true;
        wrapped.data.data = packed;
        wrapped.data.len = packedLen;
        IHS_SessionChannel *channel = IHS_SessionChannelForType(session, IHS_SessionChannelTypeControl);
        ret = IHS_SessionChannelControlSend(channel, k_EStreamControlRemoteHID,
                                            (const ProtobufCMessage *) &wrapped, IHS_PACKET_ID_NEXT);
        free(packed);
    }
    return ret;
}

static void HandleDeviceOpen(IHS_SessionChannel *channel, IHS_HIDManager *manager,
                             const CHIDMessageToRemote *message) {
    CHIDMessageToRemote__DeviceOpen *cmd = message->device_open;
    IHS_HIDManagedDevice *managed = IHS_HIDManagerOpenDevice(manager, cmd->info->path);
    if (managed == NULL) {
        SendRequestCodeResponse(channel, message->request_id, -1);
        IHS_SessionLog(channel->session, IHS_LogLevelDebug, "HID",
                       "Message %u: Open(path=%s, product_string=%s) => (nil)",
                       message->request_id, cmd->info->path, cmd->info->product_string);
        return;
    }
    // Send managed ID as result
    CHIDMessageFromRemote__RequestResponse response = CHIDMESSAGE_FROM_REMOTE__REQUEST_RESPONSE__INIT;
    PROTOBUF_C_SET_VALUE(response, request_id, message->request_id);
    PROTOBUF_C_SET_VALUE(response, result, managed->id);
    SendRequestResponse(channel, &response);
    IHS_SessionLog(channel->session, IHS_LogLevelDebug, "HID", "Message %u: Open(path=%s, product_string=%s) => id=%u",
                   message->request_id, cmd->info->path, cmd->info->product_string, managed->id);
}

static void HandleDeviceClose(IHS_SessionChannel *channel, IHS_HIDManager *manager,
                              const CHIDMessageToRemote *message) {
    CHIDMessageToRemote__DeviceClose *cmd = message->device_close;
    IHS_HIDManagedDevice *managed = IHS_HIDManagerFindDeviceByID(manager, cmd->device);
    IHS_SessionLog(channel->session, IHS_LogLevelDebug, "HID", "Message %u: Close(id=%u), found=%u",
                   message->request_id, cmd->device, managed != NULL);
    // If the device got closed on client side, it should be NULL here.
    if (managed != NULL) {
        IHS_HIDManagedDeviceClose(managed);
    }
}

static void HandleDeviceWrite(IHS_SessionChannel *channel, IHS_HIDManager *manager,
                              const CHIDMessageToRemote *message) {
    CHIDMessageToRemote__DeviceWrite *cmd = message->device_write;
    IHS_HIDManagedDevice *managed = IHS_HIDManagerFindDeviceByID(manager, cmd->device);
    if (managed == NULL) {
        IHS_SessionLog(channel->session, IHS_LogLevelVerbose, "HID", "Message %u: Write(id=%u) => (no device)",
                       message->request_id, cmd->device);
        return;
    }
    int ret = IHS_HIDDeviceWrite(managed->device, cmd->data.data, cmd->data.len);
    IHS_SessionLog(channel->session, IHS_LogLevelVerbose, "HID", "Message %u: Write(id=%u) => %d", message->request_id,
                   cmd->device, ret);
}

static void HandleDeviceRead(IHS_SessionChannel *channel, IHS_HIDManager *manager, const CHIDMessageToRemote *message) {
    CHIDMessageToRemote__DeviceRead *cmd = message->device_read;
    IHS_HIDManagedDevice *managed = IHS_HIDManagerFindDeviceByID(manager, cmd->device);
    if (managed == NULL) {
        SendRequestCodeResponse(channel, message->request_id, -1);
        IHS_SessionLog(channel->session, IHS_LogLevelVerbose, "HID", "Message %u: Read(id=%u) => (no device)",
                       message->request_id, cmd->device);
        return;
    }
    IHS_Buffer str = IHS_BUFFER_INIT(cmd->length, 255);
    int result = IHS_HIDDeviceRead(managed->device, &str, cmd->length, cmd->timeout_ms);

    CHIDMessageFromRemote__RequestResponse response = CHIDMESSAGE_FROM_REMOTE__REQUEST_RESPONSE__INIT;
    PROTOBUF_C_SET_VALUE(response, request_id, message->request_id);
    PROTOBUF_C_SET_VALUE(response, result, result);
    if (result == 0) {
        response.has_data = true;
        response.data.data = IHS_BufferPointer(&str);
        response.data.len = str.size;
    }
    IHS_SessionLog(channel->session, IHS_LogLevelVerbose, "HID", "Message %u: Read(id=%u) => ret=%d, %u byte(s)",
                   message->request_id, cmd->device, response.result, response.data.len);
    SendRequestResponse(channel, &response);
}

static void HandleDeviceSendFeatureReport(IHS_SessionChannel *channel, IHS_HIDManager *manager,
                                          const CHIDMessageToRemote *message) {
    CHIDMessageToRemote__DeviceSendFeatureReport *cmd = message->device_send_feature_report;
    IHS_HIDManagedDevice *managed = IHS_HIDManagerFindDeviceByID(manager, cmd->device);
    if (managed == NULL) {
        IHS_SessionLog(channel->session, IHS_LogLevelVerbose, "HID",
                       "Message %u: SendFeatureReport(id=%u) => (no device)",
                       message->request_id, cmd->device);
        return;
    }
    // Mirrors Steam's case 6 in CStreamPlayer::OnRemoteHIDMessage (0x228a64):
    // retry up to 50 times with 2 ms between attempts. Protects force-feedback /
    // LED commands from transient HID write failures (busy device, USB hiccup).
    int ret = -1;
    for (int attempt = 0; attempt < 50; attempt++) {
        ret = IHS_HIDDeviceSendFeatureReport(managed->device, cmd->data.data, cmd->data.len);
        if (ret >= 0) break;
        usleep(2000);
    }
    if (ret < 0) {
        IHS_SessionLog(channel->session, IHS_LogLevelWarn, "HID",
                       "Message %u: SendFeatureReport(id=%u) failed after 50 attempts: %d",
                       message->request_id, cmd->device, ret);
    }
}

static void HandleDeviceGetFeatureReport(IHS_SessionChannel *channel, IHS_HIDManager *manager,
                                         const CHIDMessageToRemote *message) {
    CHIDMessageToRemote__DeviceGetFeatureReport *cmd = message->device_get_feature_report;
    IHS_HIDManagedDevice *managed = IHS_HIDManagerFindDeviceByID(manager, cmd->device);

    if (managed == NULL) {
        SendRequestCodeResponse(channel, message->request_id, -1);
        IHS_SessionLog(channel->session, IHS_LogLevelVerbose, "HID",
                       "Message %u: GetFeatureReport(id=%u) => (no device)",
                       message->request_id, cmd->device);
        return;
    }

    IHS_Buffer str = IHS_BUFFER_INIT(cmd->length, 255);
    int result = IHS_HIDDeviceGetFeatureReport(managed->device, cmd->report_number.data, cmd->report_number.len,
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
    IHS_SessionLog(channel->session, IHS_LogLevelVerbose, "HID",
                   "Message %u: GetFeatureReport(id=%u) => ret=%d, %u byte(s)",
                   message->request_id, cmd->device, response.result, response.data.len);
    IHS_BufferClear(&str, true);
}

static void HandleDeviceGetVendorString(IHS_SessionChannel *channel, IHS_HIDManager *manager,
                                        const CHIDMessageToRemote *message) {
    CHIDMessageToRemote__DeviceGetVendorString *cmd = message->device_get_vendor_string;
    IHS_HIDManagedDevice *managed = IHS_HIDManagerFindDeviceByID(manager, cmd->device);

    if (managed == NULL) {
        SendRequestCodeResponse(channel, message->request_id, -1);
        IHS_SessionLog(channel->session, IHS_LogLevelVerbose, "HID",
                       "Message %u: GetVendorString(id=%u) => (no device)",
                       message->request_id, cmd->device);
        return;
    }

    IHS_Buffer str = IHS_BUFFER_INIT(0, 255);
    int result = IHS_HIDDeviceGetVendorString(managed->device, &str);

    CHIDMessageFromRemote__RequestResponse response = CHIDMESSAGE_FROM_REMOTE__REQUEST_RESPONSE__INIT;
    PROTOBUF_C_SET_VALUE(response, request_id, message->request_id);
    PROTOBUF_C_SET_VALUE(response, result, result);
    response.has_data = true;
    response.data.data = IHS_BufferPointer(&str);
    response.data.len = str.size;
    SendRequestResponse(channel, &response);
    IHS_SessionLog(channel->session, IHS_LogLevelVerbose, "HID", "Message %u: GetVendorString(id=%u) => \"%s\"",
                   message->request_id, cmd->device, IHS_BufferPointer(&str));
    IHS_BufferClear(&str, true);
}

static void HandleDeviceGetProductString(IHS_SessionChannel *channel, IHS_HIDManager *manager,
                                         const CHIDMessageToRemote *message) {
    CHIDMessageToRemote__DeviceGetProductString *cmd = message->device_get_product_string;
    IHS_HIDManagedDevice *managed = IHS_HIDManagerFindDeviceByID(manager, cmd->device);

    if (managed == NULL) {
        SendRequestCodeResponse(channel, message->request_id, -1);
        IHS_SessionLog(channel->session, IHS_LogLevelVerbose, "HID",
                       "Message %u: GetProductString(id=%u) => (no device)",
                       message->request_id, cmd->device);
        return;
    }

    IHS_Buffer str = IHS_BUFFER_INIT(0, 255);
    int result = IHS_HIDDeviceGetProductString(managed->device, &str);

    CHIDMessageFromRemote__RequestResponse response = CHIDMESSAGE_FROM_REMOTE__REQUEST_RESPONSE__INIT;
    PROTOBUF_C_SET_VALUE(response, request_id, message->request_id);
    PROTOBUF_C_SET_VALUE(response, result, result);
    response.has_data = true;
    response.data.data = IHS_BufferPointer(&str);
    response.data.len = str.size;
    SendRequestResponse(channel, &response);
    IHS_SessionLog(channel->session, IHS_LogLevelVerbose, "HID", "Message %u: GetProductString(id=%u) => \"%s\"",
                   message->request_id, cmd->device, IHS_BufferPointer(&str));
    IHS_BufferClear(&str, true);
}

static void HandleDeviceGetSerialNumberString(IHS_SessionChannel *channel, IHS_HIDManager *manager,
                                              const CHIDMessageToRemote *message) {
    CHIDMessageToRemote__DeviceGetSerialNumberString *cmd = message->device_get_serial_number_string;
    IHS_HIDManagedDevice *managed = IHS_HIDManagerFindDeviceByID(manager, cmd->device);

    if (managed == NULL) {
        SendRequestCodeResponse(channel, message->request_id, -1);
        IHS_SessionLog(channel->session, IHS_LogLevelVerbose, "HID",
                       "Message %u: GetSerialNumberString(id=%u) => (no device)",
                       message->request_id, cmd->device);
        return;
    }

    IHS_Buffer str = IHS_BUFFER_INIT(0, 255);
    int result = IHS_HIDDeviceGetSerialNumberString(managed->device, &str);

    CHIDMessageFromRemote__RequestResponse response = CHIDMESSAGE_FROM_REMOTE__REQUEST_RESPONSE__INIT;
    PROTOBUF_C_SET_VALUE(response, request_id, message->request_id);
    PROTOBUF_C_SET_VALUE(response, result, result);
    response.has_data = true;
    response.data.data = IHS_BufferPointer(&str);
    response.data.len = str.size;
    SendRequestResponse(channel, &response);
    IHS_SessionLog(channel->session, IHS_LogLevelVerbose, "HID", "Message %u: GetSerialNumberString(id=%u) => \"%s\"",
                   message->request_id, cmd->device, IHS_BufferPointer(&str));
    IHS_BufferClear(&str, true);
}

static void HandleDeviceStartInputReports(IHS_SessionChannel *channel, IHS_HIDManager *manager,
                                          const CHIDMessageToRemote *message) {
    CHIDMessageToRemote__DeviceStartInputReports *cmd = message->device_start_input_reports;
    IHS_HIDManagedDevice *managed = IHS_HIDManagerFindDeviceByID(manager, cmd->device);

    if (managed == NULL) {
        IHS_SessionLog(channel->session, IHS_LogLevelVerbose, "HID",
                       "Message %u: StartInputReports(id=%u) => (no device)",
                       message->request_id, cmd->device);
        return;
    }

    IHS_HIDReportHolderSetReportLength(&managed->reportHolder, cmd->length);
    if (IHS_HIDDeviceStartInputReports(managed->device, cmd->length) == 0) {
        // Here we assume this managed has generated one full report, and send it right away.
        // This design may require change later...
        IHS_SessionHIDSendReport(channel->session);
    }
}

static void HandleDeviceRequestFullReport(IHS_SessionChannel *channel, IHS_HIDManager *manager,
                                          const CHIDMessageToRemote *message) {
    CHIDMessageToRemote__DeviceRequestFullReport *cmd = message->device_request_full_report;
    IHS_HIDManagedDevice *managed = IHS_HIDManagerFindDeviceByID(manager, cmd->device);

    if (managed == NULL) {
        IHS_SessionLog(channel->session, IHS_LogLevelVerbose, "HID",
                       "Message %u: RequestFullReport(id=%u) => (no device)",
                       message->request_id, cmd->device);
        return;
    }

    if (IHS_HIDDeviceRequestFullReport(managed->device) == 0) {
        // Here we assume this device has generated one full report, and send it right away.
        // This design may require change later...
        IHS_SessionHIDSendReport(channel->session);
    }
}

static void HandleDeviceDisconnect(IHS_SessionChannel *channel, IHS_HIDManager *manager,
                                   const CHIDMessageToRemote *message) {
    CHIDMessageToRemote__DeviceDisconnect *cmd = message->device_disconnect;
    IHS_HIDManagedDevice *managed = IHS_HIDManagerFindDeviceByID(manager, cmd->device);

    if (managed == NULL) {
        IHS_SessionLog(channel->session, IHS_LogLevelDebug, "HID",
                       "Message %u: Disconnect(id=%u) => (no device)", message->request_id, cmd->device);
        return;
    }

    IHS_HIDDeviceRequestDisconnect(managed->device, cmd->disconnectmethod, cmd->data.data, cmd->data.len);
}

static void SendRequestResponse(IHS_SessionChannel *channel, CHIDMessageFromRemote__RequestResponse *response) {
    CHIDMessageFromRemote outMessage = CHIDMESSAGE_FROM_REMOTE__INIT;
    outMessage.command_case = CHIDMESSAGE_FROM_REMOTE__COMMAND_RESPONSE;
    outMessage.response = response;
    IHS_SessionChannelControlSendHIDMsg(channel, &outMessage);
}

static void SendRequestCodeResponse(IHS_SessionChannel *channel, uint32_t requestId, int result) {
    CHIDMessageFromRemote__RequestResponse response = CHIDMESSAGE_FROM_REMOTE__REQUEST_RESPONSE__INIT;
    PROTOBUF_C_SET_VALUE(response, request_id, requestId);
    PROTOBUF_C_SET_VALUE(response, result, result);
    SendRequestResponse(channel, &response);
}

static void InfoFromHID(CHIDDeviceInfo *info, const IHS_HIDDeviceInfo *hid) {
    chiddevice_info__init(info);
    PROTOBUF_C_P_SET_VALUE(info, location, k_EDeviceLocationLocal);
    info->path = strdup(hid->path);
    info->product_string = strdup(hid->product_string);
    if (hid->serial_number) {
        info->serial_number = strdup(hid->serial_number);
    }
    if (hid->vendor_id && hid->product_id) {
        PROTOBUF_C_P_SET_VALUE(info, vendor_id, hid->vendor_id);
        PROTOBUF_C_P_SET_VALUE(info, product_id, hid->product_id);
        PROTOBUF_C_P_SET_VALUE(info, release_number, hid->product_version);
    }
    PROTOBUF_C_P_SET_VALUE(info, usage_page, 1);
    PROTOBUF_C_P_SET_VALUE(info, usage, 5/*For SDL_GameController*/);
    PROTOBUF_C_P_SET_VALUE(info, is_generic_gamepad, true);
    PROTOBUF_C_P_SET_VALUE(info, ostype, IHS_SteamOSTypeLinux);

    // Expect 0x8043ff
    IHS_HIDDeviceCaps capsBits = IHS_HID_CAP_ABXY | IHS_HID_CAP_DPAD | IHS_HID_CAP_LSTICK | IHS_HID_CAP_RSTICK |
                                 IHS_HID_CAP_STICKBTNS | IHS_HID_CAP_SHOULDERS | IHS_HID_CAP_TRIGGERS |
                                 IHS_HID_CAP_BACK | IHS_HID_CAP_START | IHS_HID_CAP_GUIDE | IHS_HID_CAP_MISC_1 |
                                 IHS_HID_CAP_XINPUT_OR_HIDAPI | IHS_HID_CAP_UNK_3 | IHS_HID_CAP_UNK_4;
    PROTOBUF_C_P_SET_VALUE(info, caps_bits, capsBits);
}