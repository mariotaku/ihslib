/*
 *  _____  _   _  _____  _  _  _
 * |_   _|| | | |/  ___|| |(_)| |     Steam
 *   | |  | |_| |\ `--. | | _ | |__     In-Home
 *   | |  |  _  | `--. \| || || '_ \      Streaming
 *  _| |_ | | | |/\__/ /| || || |_) |       Library
 *  \___/ \_| |_/\____/ |_||_||_.__/
 *
 * Copyright (c) 2026 Mariotaku <https://github.com/mariotaku>.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 */

/**
 * Drives IHS_SessionChannelControlOnHIDMsg and decodes what the control channel actually queued for
 * the wire, so the raw-HID response shape is verified by running rather than by reading the
 * reference. Nothing here starts a thread: the session is created but never connected, so queued
 * packets simply pile up in session->sendQueue where the test can take them apart.
 */

#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "session/session_pri.h"
#include "session/frame.h"
#include "session/channels/ch_control.h"
#include "hid/manager.h"
#include "hid/provider.h"
#include "hid/device.h"
#include "ihs_enumeration.h"
#include "ihs_queue.h"
#include "protobuf/pb_utils.h"

#include "test_session.h"

/**
 * IHS_QueueItem is intentionally an incomplete type that each queue's owner defines for itself, so
 * this mirrors the definition in session.c. Keep the two in step.
 */
typedef struct IHS_QueueItem {
    IHS_SessionPacket packet;
    bool retransmit;
} QueuedPacket;

#define READ_FILL 0xA7
#define FEATURE_FILL 0x5C
#define REPORT_LEN 48

static size_t readBytesToProduce = 0;
static int readReturnOverride = 0;
static size_t featureBytesToProduce = 0;

/** Stands in for the provider's view of the controller, as IHS_HIDDeviceSDL::states.current does. */
static uint8_t deviceState[REPORT_LEN];

/* ------------------------------------------------------------------ fake device */

static IHS_HIDDevice *DeviceAlloc(const struct IHS_HIDDeviceClass *cls) {
    IHS_HIDDevice *device = calloc(1, sizeof(IHS_HIDDevice));
    device->cls = cls;
    return device;
}

static void DeviceFree(IHS_HIDDevice *device) { free(device); }

static void DeviceClose(IHS_HIDDevice *device) { (void) device; }

static int DeviceWrite(IHS_HIDDevice *device, const uint8_t *data, size_t dataLen) {
    (void) device;
    (void) data;
    return (int) dataLen;
}

static int DeviceRead(IHS_HIDDevice *device, IHS_Buffer *dest, size_t length, uint32_t timeoutMs) {
    (void) device;
    (void) timeoutMs;
    size_t produce = readBytesToProduce < length ? readBytesToProduce : length;
    if (produce > 0) {
        uint8_t *pointer = IHS_BufferPointerForAppend(dest, produce);
        memset(pointer, READ_FILL, produce);
        dest->size += produce;
    }
    // Lets a case pretend the provider misreports its count.
    return readReturnOverride != 0 ? readReturnOverride : (int) produce;
}

static int DeviceGetFeatureReport(IHS_HIDDevice *device, const uint8_t *reportNumber, size_t reportNumberLen,
                                  IHS_Buffer *dest, size_t length) {
    (void) device;
    (void) reportNumberLen;
    size_t produce = featureBytesToProduce < length ? featureBytesToProduce : length;
    if (produce == 0) {
        return 0;
    }
    IHS_BufferWriteMem(dest, 0, reportNumber, 1);
    if (produce > 1) {
        IHS_BufferFillMem(dest, 1, FEATURE_FILL, produce - 1);
    }
    return (int) produce;
}

static int DeviceSendFeatureReport(IHS_HIDDevice *device, const uint8_t *data, size_t dataLen) {
    (void) device;
    (void) data;
    (void) dataLen;
    return -1;
}

/* Mirrors DeviceStartInputReports / DeviceRequestFullReport in sdl_hid_device.c: stash the current
 * state into the holder and return. Neither sends anything itself. */
static int DeviceStartInputReports(IHS_HIDDevice *device, size_t length) {
    (void) length;
    IHS_HIDDeviceReportAddFull(device, deviceState, REPORT_LEN);
    return 0;
}

static int DeviceRequestFullReport(IHS_HIDDevice *device) {
    IHS_HIDDeviceReportAddFull(device, deviceState, REPORT_LEN);
    return 0;
}

static int DeviceString(IHS_HIDDevice *device, IHS_Buffer *dest) {
    (void) device;
    IHS_BufferWriteMem(dest, 0, (const uint8_t *) "", 1);
    return 0;
}

static const IHS_HIDDeviceClass DeviceClass = {
        .alloc = DeviceAlloc,
        .free = DeviceFree,
        .close = DeviceClose,
        .write = DeviceWrite,
        .read = DeviceRead,
        .sendFeatureReport = DeviceSendFeatureReport,
        .getFeatureReport = DeviceGetFeatureReport,
        .getVendorString = DeviceString,
        .getProductString = DeviceString,
        .getSerialNumberString = DeviceString,
        .startInputReports = DeviceStartInputReports,
        .requestFullReport = DeviceRequestFullReport,
};

/* ------------------------------------------------------------------ fake provider */

static IHS_HIDProvider *ProviderAlloc(const IHS_HIDProviderClass *cls) {
    IHS_HIDProvider *provider = calloc(1, sizeof(IHS_HIDProvider));
    provider->cls = cls;
    return provider;
}

static void ProviderFree(IHS_HIDProvider *provider) { free(provider); }

static bool ProviderSupportsDevice(IHS_HIDProvider *provider, const char *path) {
    (void) provider;
    return strncmp(path, "test://", 7) == 0;
}

static IHS_HIDDevice *ProviderOpenDevice(IHS_HIDProvider *provider, const char *path) {
    (void) provider;
    (void) path;
    return IHS_HIDDeviceCreate(&DeviceClass);
}

static IHS_Enumeration *ProviderEnumerate(IHS_HIDProvider *provider) {
    (void) provider;
    return IHS_EnumerationArrayCreate(NULL, 0, 0, NULL);
}

static const IHS_HIDProviderClass ProviderClass = {
        .alloc = ProviderAlloc,
        .free = ProviderFree,
        .supportsDevice = ProviderSupportsDevice,
        .openDevice = ProviderOpenDevice,
        .enumerateDevices = ProviderEnumerate,
};

/* ------------------------------------------------------------------ wire decoding */

static uint64_t expectedSequence = 0;

/**
 * Take the single packet the control channel queued and unwrap it all the way back to the
 * RequestResponse: packet body is [control message type][encrypted CRemoteHIDMsg].
 */
static CHIDMessageFromRemote *TakeFromRemote(IHS_Session *session) {
    IHS_QueueItem *item = IHS_QueuePoll(session->sendQueue);
    assert(item != NULL);
    QueuedPacket *queued = (QueuedPacket *) item;

    IHS_Buffer *body = &queued->packet.body;
    assert(body->size > 1);
    assert(IHS_BufferPointerAt(body, 0)[0] == k_EStreamControlRemoteHID);

    IHS_Buffer cipher = *body;
    IHS_BufferOffsetBy(&cipher, 1);

    IHS_Buffer plain = IHS_BUFFER_INIT(1024, 8192);
    uint64_t actualSequence = 0;
    IHS_SessionFrameDecryptResult decrypted = IHS_SessionFrameDecrypt(session, &cipher, &plain, expectedSequence,
                                                                      &actualSequence);
    assert(decrypted == IHS_SessionFrameDecryptOK);
    expectedSequence++;

    CRemoteHIDMsg *wrapped = cremote_hidmsg__unpack(NULL, plain.size, IHS_BufferPointer(&plain));
    assert(wrapped != NULL);
    assert(wrapped->has_data);
    CHIDMessageFromRemote *fromRemote = chidmessage_from_remote__unpack(NULL, wrapped->data.len, wrapped->data.data);
    assert(fromRemote != NULL);

    cremote_hidmsg__free_unpacked(wrapped, NULL);
    IHS_BufferClear(&plain, true);
    IHS_SessionPacketClear(&queued->packet, true);
    IHS_QueueItemFree(item);

    return fromRemote;
}

static CHIDMessageFromRemote__RequestResponse *TakeResponse(IHS_Session *session, CHIDMessageFromRemote **outOwner) {
    CHIDMessageFromRemote *fromRemote = TakeFromRemote(session);
    assert(fromRemote->command_case == CHIDMESSAGE_FROM_REMOTE__COMMAND_RESPONSE);
    *outOwner = fromRemote;
    return fromRemote->response;
}

static void AssertQueueEmpty(IHS_Session *session) {
    assert(IHS_QueueIsEmpty(session->sendQueue));
}

/* ------------------------------------------------------------------ cases */

static void SendRead(IHS_SessionChannel *channel, uint32_t deviceId, uint32_t requestId, uint32_t length) {
    CHIDMessageToRemote__DeviceRead read = CHIDMESSAGE_TO_REMOTE__DEVICE_READ__INIT;
    PROTOBUF_C_SET_VALUE(read, device, deviceId);
    PROTOBUF_C_SET_VALUE(read, length, length);
    PROTOBUF_C_SET_VALUE(read, timeout_ms, 0);
    CHIDMessageToRemote message = CHIDMESSAGE_TO_REMOTE__INIT;
    PROTOBUF_C_SET_VALUE(message, request_id, requestId);
    message.command_case = CHIDMESSAGE_TO_REMOTE__COMMAND_DEVICE_READ;
    message.device_read = &read;
    IHS_SessionChannelControlOnHIDMsg(channel, &message);
}

static void SendGetFeatureReport(IHS_SessionChannel *channel, uint32_t deviceId, uint32_t requestId, uint32_t length) {
    uint8_t reportNumber[1] = {0x04};
    CHIDMessageToRemote__DeviceGetFeatureReport get = CHIDMESSAGE_TO_REMOTE__DEVICE_GET_FEATURE_REPORT__INIT;
    PROTOBUF_C_SET_VALUE(get, device, deviceId);
    PROTOBUF_C_SET_VALUE(get, length, length);
    get.has_report_number = true;
    get.report_number.data = reportNumber;
    get.report_number.len = sizeof(reportNumber);
    CHIDMessageToRemote message = CHIDMESSAGE_TO_REMOTE__INIT;
    PROTOBUF_C_SET_VALUE(message, request_id, requestId);
    message.command_case = CHIDMESSAGE_TO_REMOTE__COMMAND_DEVICE_GET_FEATURE_REPORT;
    message.device_get_feature_report = &get;
    IHS_SessionChannelControlOnHIDMsg(channel, &message);
}

static void SendStartInputReports(IHS_SessionChannel *channel, uint32_t deviceId, uint32_t requestId,
                                  uint32_t length) {
    CHIDMessageToRemote__DeviceStartInputReports start = CHIDMESSAGE_TO_REMOTE__DEVICE_START_INPUT_REPORTS__INIT;
    PROTOBUF_C_SET_VALUE(start, device, deviceId);
    PROTOBUF_C_SET_VALUE(start, length, length);
    CHIDMessageToRemote message = CHIDMESSAGE_TO_REMOTE__INIT;
    PROTOBUF_C_SET_VALUE(message, request_id, requestId);
    message.command_case = CHIDMESSAGE_TO_REMOTE__COMMAND_DEVICE_START_INPUT_REPORTS;
    message.device_start_input_reports = &start;
    IHS_SessionChannelControlOnHIDMsg(channel, &message);
}

static void SendRequestFullReport(IHS_SessionChannel *channel, uint32_t deviceId, uint32_t requestId) {
    CHIDMessageToRemote__DeviceRequestFullReport full = CHIDMESSAGE_TO_REMOTE__DEVICE_REQUEST_FULL_REPORT__INIT;
    PROTOBUF_C_SET_VALUE(full, device, deviceId);
    CHIDMessageToRemote message = CHIDMESSAGE_TO_REMOTE__INIT;
    PROTOBUF_C_SET_VALUE(message, request_id, requestId);
    message.command_case = CHIDMESSAGE_TO_REMOTE__COMMAND_DEVICE_REQUEST_FULL_REPORT;
    message.device_request_full_report = &full;
    IHS_SessionChannelControlOnHIDMsg(channel, &message);
}

/**
 * Run the session's timers until the HID poll task has had its turn. There is no timer thread, so
 * the test drives it exactly as IHS_Base's worker does — sleep to the deadline, then run what is due.
 */
static void PumpPollTick(IHS_Session *session) {
    IHS_Timer *timers = session->base.timers;
    uint64_t deadline = IHS_TimerNextDeadline(timers);
    assert(deadline != 0);
    uint64_t now = IHS_TimerNow();
    if (deadline > now) {
        usleep((useconds_t) (deadline - now) * 1000);
    }
    IHS_TimerRunPending(timers);
}

/** Unwrap one queued packet as a batch of device input reports. */
static CHIDMessageFromRemote__DeviceInputReports *TakeReports(IHS_Session *session,
                                                              CHIDMessageFromRemote **outOwner) {
    CHIDMessageFromRemote *fromRemote = TakeFromRemote(session);
    assert(fromRemote->command_case == CHIDMESSAGE_FROM_REMOTE__COMMAND_REPORTS);
    *outOwner = fromRemote;
    return fromRemote->reports;
}

int main(void) {
    IHS_Init();
    IHS_Session *session = IHS_TestSessionCreate();
    // SendHIDMsg mirrors Steam's IsStreaming + BStreamingInput gate and drops everything unless the
    // session is live and the server has enabled input.
    session->state.streamingInput = true;
    session->state.connectionState = IHS_SessionConnectionStateConnected;
    IHS_SessionChannel *control = session->channels[IHS_SessionChannelIdControl];
    assert(control != NULL);

    IHS_HIDProvider *provider = IHS_SessionHIDProviderCreate(&ProviderClass);
    IHS_HIDManagerAddProvider(session->hidManager, provider);
    IHS_HIDManagedDevice *managed = IHS_HIDManagerOpenDevice(session->hidManager, "test://0");
    assert(managed != NULL);
    uint32_t deviceId = managed->id;

    CHIDMessageFromRemote *owner;
    CHIDMessageFromRemote__RequestResponse *response;

    // A successful read attaches the data, with the length taken from the return value.
    readBytesToProduce = 30;
    SendRead(control, deviceId, 11, 30);
    response = TakeResponse(session, &owner);
    assert(response->request_id == 11);
    assert(response->result == 30);
    assert(response->has_data);
    assert(response->data.len == 30);
    for (size_t i = 0; i < response->data.len; i++) {
        assert(response->data.data[i] == READ_FILL);
    }
    chidmessage_from_remote__free_unpacked(owner, NULL);
    AssertQueueEmpty(session);

    // A read that produced nothing still answers, but with no data field — the reference gates on
    // `0 < result`, so a zero-byte read must not carry an empty payload.
    readBytesToProduce = 0;
    SendRead(control, deviceId, 12, 30);
    response = TakeResponse(session, &owner);
    assert(response->request_id == 12);
    assert(response->result == 0);
    assert(!response->has_data);
    chidmessage_from_remote__free_unpacked(owner, NULL);
    AssertQueueEmpty(session);

    // A provider over-reporting its count must not make us ship uninitialised buffer: only the
    // bytes it actually wrote go out, however large a number it claims.
    readBytesToProduce = 4;
    readReturnOverride = 9999;
    SendRead(control, deviceId, 13, 8);
    response = TakeResponse(session, &owner);
    assert(response->request_id == 13);
    assert(response->has_data);
    assert(response->data.len == 4);
    for (size_t i = 0; i < response->data.len; i++) {
        assert(response->data.data[i] == READ_FILL);
    }
    chidmessage_from_remote__free_unpacked(owner, NULL);
    readReturnOverride = 0;
    AssertQueueEmpty(session);

    // An unknown device is refused with -1 and no data.
    SendRead(control, deviceId + 4242, 14, 30);
    response = TakeResponse(session, &owner);
    assert(response->request_id == 14);
    assert(response->result == -1);
    assert(!response->has_data);
    chidmessage_from_remote__free_unpacked(owner, NULL);
    AssertQueueEmpty(session);

    // A length past the ceiling is refused rather than tripping the buffer capacity assert.
    readBytesToProduce = 16;
    SendRead(control, deviceId, 15, 1024 * 1024);
    response = TakeResponse(session, &owner);
    assert(response->request_id == 15);
    assert(response->result == -1);
    assert(!response->has_data);
    chidmessage_from_remote__free_unpacked(owner, NULL);
    AssertQueueEmpty(session);

    // get_feature_report follows the same rule: positive count, data length from the return value.
    featureBytesToProduce = 21;
    SendGetFeatureReport(control, deviceId, 21, 65);
    response = TakeResponse(session, &owner);
    assert(response->request_id == 21);
    assert(response->result == 21);
    assert(response->has_data);
    assert(response->data.len == 21);
    assert(response->data.data[0] == 0x04);
    assert(response->data.data[1] == FEATURE_FILL);
    chidmessage_from_remote__free_unpacked(owner, NULL);
    AssertQueueEmpty(session);

    featureBytesToProduce = 0;
    SendGetFeatureReport(control, deviceId, 22, 65);
    response = TakeResponse(session, &owner);
    assert(response->request_id == 22);
    assert(response->result == 0);
    assert(!response->has_data);
    chidmessage_from_remote__free_unpacked(owner, NULL);
    AssertQueueEmpty(session);

    SendGetFeatureReport(control, deviceId, 23, 1024 * 1024);
    response = TakeResponse(session, &owner);
    assert(response->request_id == 23);
    assert(response->result == -1);
    assert(!response->has_data);
    chidmessage_from_remote__free_unpacked(owner, NULL);
    AssertQueueEmpty(session);

    // ---------------------------------------------------------------- cases 11 and 12
    //
    // Neither sends anything from the handler. Case 11 of CStreamPlayer::OnRemoteHIDMessage starts
    // the stream and case 12 sets the full-report latch; both leave the report to the poll tick, so
    // however many arrive inside one interval they leave as a single message.

    memset(deviceState, 0x11, sizeof(deviceState));
    SendStartInputReports(control, deviceId, 31, REPORT_LEN);
    // Nothing on the wire yet: no response for case 11, and no report either.
    AssertQueueEmpty(session);

    PumpPollTick(session);
    CHIDMessageFromRemote__DeviceInputReports *reports;
    reports = TakeReports(session, &owner);
    assert(reports->n_device_reports == 1);
    assert(reports->device_reports[0]->device == deviceId);
    assert(reports->device_reports[0]->n_reports == 1);
    assert(reports->device_reports[0]->reports[0]->has_full_report);
    assert(reports->device_reports[0]->reports[0]->full_report.len == REPORT_LEN);
    assert(reports->device_reports[0]->reports[0]->full_report.data[0] == 0x11);
    chidmessage_from_remote__free_unpacked(owner, NULL);
    AssertQueueEmpty(session);

    // Two full-report requests inside one interval, with the state unchanged between them. Both
    // must survive the identical-state dedup — the latch outranks it — and both must ride out in
    // the same message rather than one send each.
    SendRequestFullReport(control, deviceId, 32);
    AssertQueueEmpty(session);
    SendRequestFullReport(control, deviceId, 33);
    AssertQueueEmpty(session);

    PumpPollTick(session);
    reports = TakeReports(session, &owner);
    assert(reports->n_device_reports == 1);
    assert(reports->device_reports[0]->n_reports == 2);
    for (size_t i = 0; i < reports->device_reports[0]->n_reports; i++) {
        assert(reports->device_reports[0]->reports[i]->has_full_report);
        assert(reports->device_reports[0]->reports[i]->full_report.len == REPORT_LEN);
    }
    chidmessage_from_remote__free_unpacked(owner, NULL);
    // One message for the pair, and nothing left over.
    AssertQueueEmpty(session);

    // An unknown device is ignored outright: no report, and still no response.
    SendRequestFullReport(control, deviceId + 4242, 34);
    AssertQueueEmpty(session);
    PumpPollTick(session);
    AssertQueueEmpty(session);

    IHS_HIDManagedDeviceClose(managed);
    IHS_HIDManagerRemoveProvider(session->hidManager, provider);
    IHS_SessionHIDProviderDestroy(provider);
    IHS_SessionDestroy(session);
    IHS_Quit();
    printf("control_hid tests OK\n");
    return 0;
}
