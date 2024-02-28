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

#include "ihslib/client.h"

#include <stdint.h>
#include <stdlib.h>
#include <memory.h>

#include "protobuf/discovery.pb-c.h"
#include "endianness.h"
#include "client_pri.h"
#include "crypto.h"
#include "base.h"
#include "ihs_buffer.h"
#include "ihs_buffer_ext.h"
#include "protobuf/pb_utils.h"

static void ClientInitialized(IHS_Base *base, void *context);

static const unsigned char PACKET_MAGIC[8] = {0xff, 0xff, 0xff, 0xff, 0x21, 0x4c, 0x5f, 0xa0};

static void ClientRecvCallback(IHS_Base *base, const IHS_SocketAddress *address, IHS_Buffer *data);

static const ProtobufCMessageDescriptor *MessageDescriptors[k_ERemoteDeviceStreamingProgress + 1] = {
        &cmsg_remote_client_broadcast_discovery__descriptor,
        &cmsg_remote_client_broadcast_status__descriptor,
        NULL,
        &cmsg_remote_device_authorization_request__descriptor,
        &cmsg_remote_device_authorization_response__descriptor,
        &cmsg_remote_device_streaming_request__descriptor,
        &cmsg_remote_device_streaming_response__descriptor,
        &cmsg_remote_device_proof_request__descriptor,
        &cmsg_remote_device_proof_response__descriptor,
        &cmsg_remote_device_authorization_cancel_request__descriptor,
        &cmsg_remote_device_streaming_cancel_request__descriptor,
        &cmsg_remote_client_broadcast_client_iddeconflict__descriptor,
        &cmsg_remote_device_stream_transport_signal__descriptor,
        &cmsg_remote_device_streaming_progress__descriptor,
};

static IHS_BaseRunCallbacks ClientRunCallbacks = {
        .initialized = ClientInitialized,
};

IHS_Client *IHS_ClientCreate(const IHS_ClientConfig *config) {
    IHS_Client *client = malloc(sizeof(IHS_Client));
    memset(client, 0, sizeof(IHS_Client));
    IHS_BaseInit(&client->base, config, ClientRecvCallback, true);
    IHS_BaseSetRunCallbacks(&client->base, &ClientRunCallbacks, NULL);
    client->timers = IHS_TimerCreate();

    client->privCallbacks.discovery = IHS_ClientDiscoveryCallback;
    client->privCallbacks.authorization = IHS_ClientAuthorizationCallback;
    client->privCallbacks.streaming = IHS_ClientStreamingCallback;

    IHS_BaseStartWorker(&client->base, "IHSClient");
    return client;
}

void IHS_ClientSetLogFunction(IHS_Client *client, IHS_LogFunction *logFunction) {
    IHS_BaseSetLogFunction(&client->base, logFunction);
}

void IHS_ClientStop(IHS_Client *client) {
    IHS_BaseInterruptWorker(&client->base);
}

void IHS_ClientThreadedJoin(IHS_Client *client) {
    IHS_BaseWaitWorker(&client->base);
}

void IHS_ClientDestroy(IHS_Client *client) {
    IHS_TimerDestroy(client->timers);
    IHS_ClientLog(client, IHS_LogLevelInfo, "Client", "Destroying client, bye!");
    IHS_BaseDestroy(&client->base);
    free(client);
}

void IHS_ClientSetDiscoveryCallbacks(IHS_Client *client, const IHS_ClientDiscoveryCallbacks *callbacks, void *context) {
    client->callbacks.discovery = callbacks;
    client->callbackContexts.discovery = context;
}

void IHS_ClientSetAuthorizationCallbacks(IHS_Client *client, const IHS_ClientAuthorizationCallbacks *callbacks,
                                         void *context) {
    client->callbacks.authorization = callbacks;
    client->callbackContexts.authorization = context;
}

void IHS_ClientSetStreamingCallbacks(IHS_Client *client, const IHS_ClientStreamingCallbacks *callbacks, void *context) {
    client->callbacks.streaming = callbacks;
    client->callbackContexts.streaming = context;
}

const char *IHS_ClientError(IHS_Client *client) {
    return "";
}

bool IHS_ClientSend(IHS_Client *client, IHS_SocketAddress address, ERemoteClientBroadcastMsg type,
                    ProtobufCMessage *message) {
    CMsgRemoteClientBroadcastHeader header;
    cmsg_remote_client_broadcast_header__init(&header);
    header.has_client_id = 1;
    header.client_id = client->base.deviceId;
    header.has_msg_type = 1;
    header.msg_type = type;
    size_t header_size = cmsg_remote_client_broadcast_header__get_packed_size(&header);

    IHS_Buffer buf;
    IHS_BufferInit(&buf, 1024, 2048);
    IHS_BufferAppendMem(&buf, PACKET_MAGIC, sizeof(PACKET_MAGIC));
    IHS_BufferAppendUInt32LE(&buf, header_size);
    cmsg_remote_client_broadcast_header__pack(&header, IHS_BufferPointerForAppend(&buf, header_size));
    buf.size += header_size;
    if (message != NULL) {
        size_t payload_size = protobuf_c_message_get_packed_size(message);
        IHS_BufferAppendUInt32LE(&buf, payload_size);
        protobuf_c_message_pack(message, IHS_BufferPointerForAppend(&buf, payload_size));
        buf.size += payload_size;
    } else {
        IHS_BufferAppendUInt32LE(&buf, 0);
    }

    bool ret = IHS_BaseSend(&client->base, address, &buf);
    IHS_BufferClear(&buf, true);
    return ret;
}

bool IHS_ClientBroadcast(IHS_Client *client, ERemoteClientBroadcastMsg type,
                         ProtobufCMessage *message) {
    static const IHS_SocketAddress address = {{.v4={IHS_IPAddressFamilyIPv4, {0xFF, 0xFF, 0xFF, 0xFF}}}, 27036};
    return IHS_ClientSend(client, address, type, message);
}

static void ClientRecvCallback(IHS_Base *base, const IHS_SocketAddress *address, IHS_Buffer *data) {
    if (memcmp(IHS_BufferPointer(data), PACKET_MAGIC, sizeof(PACKET_MAGIC)) != 0) {
        IHS_BaseLog(base, IHS_LogLevelDebug, "Client", "Unrecognized packet!");
        return;
    }
    IHS_BufferOffsetBy(data, sizeof(PACKET_MAGIC));
    uint32_t header_size, payload_size;
    IHS_BufferOffsetBy(data, (int) IHS_ReadUInt32LE(IHS_BufferPointer(data), &header_size));
    CMsgRemoteClientBroadcastHeader *header = IHS_UNPACK_BUFFER_SIZE(cmsg_remote_client_broadcast_header__unpack, data,
                                                                     header_size);
    IHS_BufferOffsetBy(data, (int) header_size);
    IHS_BufferOffsetBy(data, (int) IHS_ReadUInt32LE(IHS_BufferPointer(data), &payload_size));
    ERemoteClientBroadcastMsg type = header->msg_type;
    const ProtobufCMessageDescriptor *descriptor = MessageDescriptors[type];
    ProtobufCMessage *message = descriptor ? protobuf_c_message_unpack(descriptor, NULL, payload_size,
                                                                       IHS_BufferPointer(data)) : NULL;
    IHS_Client *client = (IHS_Client *) base;
    switch (type) {
        case k_ERemoteClientBroadcastMsgDiscovery:
        case k_ERemoteClientBroadcastMsgStatus:
        case k_ERemoteClientBroadcastMsgOffline:
        case k_ERemoteClientBroadcastMsgClientIDDeconflict:
            client->privCallbacks.discovery(client, address, header, message);
            break;
        case k_ERemoteDeviceAuthorizationRequest:
        case k_ERemoteDeviceAuthorizationResponse:
        case k_ERemoteDeviceAuthorizationCancelRequest:
            client->privCallbacks.authorization(client, address, header, message);
            break;
        case k_ERemoteDeviceStreamingRequest:
        case k_ERemoteDeviceStreamingResponse:
        case k_ERemoteDeviceStreamingProgress:
        case k_ERemoteDeviceStreamingCancelRequest:
        case k_ERemoteDeviceProofRequest:
        case k_ERemoteDeviceProofResponse:
            client->privCallbacks.streaming(client, address, header, message);
            break;
        default:
            break;
    }
    cmsg_remote_client_broadcast_header__free_unpacked(header, NULL);
    protobuf_c_message_free_unpacked(message, NULL);
}

static void ClientInitialized(IHS_Base *base, void *context) {
    (void) context;
    IHS_UDPSocketSetBlocking(base->socket, true);
    IHS_UDPSocketSetRecvTimeout(base->socket, 10000 /* 10ms */);
}

