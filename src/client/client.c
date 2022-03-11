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

#include "ihslib/client.h"

#include <stdint.h>
#include <malloc.h>
#include <memory.h>

#include <uv.h>

#include "protobuf/discovery.pb-c.h"
#include "endianness.h"
#include "client_pri.h"
#include "crypto.h"
#include "base.h"


static const unsigned char PACKET_MAGIC[8] = {0xff, 0xff, 0xff, 0xff, 0x21, 0x4c, 0x5f, 0xa0};

static void ClientRecvCallback(uv_udp_t *handle, ssize_t nread, uv_buf_t buf, struct sockaddr *addr, unsigned flags);

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
        NULL,
        &cmsg_remote_device_stream_transport_signal__descriptor,
        &cmsg_remote_device_streaming_progress__descriptor,
};

IHS_Client *IHS_ClientCreate(const IHS_ClientConfig *config) {
    IHS_Client *client = malloc(sizeof(IHS_Client));
    memset(client, 0, sizeof(IHS_Client));
    IHS_BaseInit(&client->base, config, ClientRecvCallback, true);

    client->privCallbacks.discovery = IHS_ClientDiscoveryCallback;
    client->privCallbacks.authorization = IHS_ClientAuthorizationCallback;
    client->privCallbacks.streaming = IHS_ClientStreamingCallback;
    return client;
}

void IHS_ClientRun(IHS_Client *client) {
    IHS_BaseRun(&client->base);
}

void IHS_ClientStop(IHS_Client *client) {
    IHS_BaseStop(&client->base);
}

void IHS_ClientThreadedStart(IHS_Client *client) {
    IHS_BaseThreadedRun(&client->base);
}

void IHS_ClientThreadedJoin(IHS_Client *client) {
    IHS_BaseThreadedJoin(&client->base);
}

void IHS_ClientDestroy(IHS_Client *client) {
    IHS_BaseFree(&client->base);
    free(client);
}

void IHS_ClientSetCallbacks(IHS_Client *client, const IHS_ClientCallbacks *callbacks, void *context) {
    client->callbacks = *callbacks;
    client->callbacksContext = context;
}

const char *IHS_ClientError(IHS_Client *client) {
    uv_err_t err = uv_last_error(client->base.loop);
    return uv_err_name(err);
}

bool IHS_ClientSend(IHS_Client *client, IHS_HostAddress address, ERemoteClientBroadcastMsg type,
                    ProtobufCMessage *message) {
    CMsgRemoteClientBroadcastHeader header;
    cmsg_remote_client_broadcast_header__init(&header);
    header.has_client_id = 1;
    header.client_id = client->base.deviceId;
    header.has_msg_type = 1;
    header.msg_type = type;
    size_t header_size = cmsg_remote_client_broadcast_header__get_packed_size(&header);
    size_t payload_size = message ? protobuf_c_message_get_packed_size(message) : 0;

    uint8_t pkt_data[1024];
    ProtobufCBufferSimple buf = PROTOBUF_C_BUFFER_SIMPLE_INIT(pkt_data);
    protobuf_c_buffer_simple_append((ProtobufCBuffer *) &buf, sizeof(PACKET_MAGIC), PACKET_MAGIC);
    IHS_AppendUInt32LEToBuffer(&buf, header_size);
    cmsg_remote_client_broadcast_header__pack_to_buffer(&header, (ProtobufCBuffer *) &buf);
    IHS_AppendUInt32LEToBuffer(&buf, payload_size);
    if (message) {
        protobuf_c_message_pack_to_buffer(message, (ProtobufCBuffer *) &buf);
    }

    return IHS_BaseSend(&client->base, address, buf.data, buf.len);
}

static void ClientRecvCallback(uv_udp_t *handle, ssize_t nread, uv_buf_t buf,
                               struct sockaddr *addr, unsigned flags) {
    if (!nread) {
        goto cleanup;
    }
    size_t offset = 0;
    if (memcmp(&buf.base[offset], PACKET_MAGIC, sizeof(PACKET_MAGIC)) != 0) {
        IHS_ClientLog(handle->loop->data, IHS_BaseLogLevelDebug, "Unrecognized packet!");
        goto cleanup;
    }
    IHS_HostIP address;
    switch (addr->sa_family) {
        case AF_INET:
            address.type = IHS_HostIPv4;
            address.value.v4 = ((struct sockaddr_in *) addr)->sin_addr;
            break;
        case AF_INET6:
            address.type = IHS_HostIPv6;
            address.value.v6 = ((struct sockaddr_in6 *) addr)->sin6_addr;
            break;
    }
    offset += sizeof(PACKET_MAGIC);
    uint32_t header_size, payload_size;
    offset += IHS_ReadUInt32LE((uint8_t *) &buf.base[offset], &header_size);
    CMsgRemoteClientBroadcastHeader *header = cmsg_remote_client_broadcast_header__unpack(NULL, header_size,
                                                                                          (uint8_t *) &
                                                                                                  buf.base[offset]);
    offset += header_size;
    offset += IHS_ReadUInt32LE((uint8_t *) &buf.base[offset], &payload_size);
    ERemoteClientBroadcastMsg type = header->msg_type;
    const ProtobufCMessageDescriptor *descriptor = MessageDescriptors[type];
    ProtobufCMessage *message = descriptor ? protobuf_c_message_unpack(descriptor, NULL, payload_size,
                                                                       (uint8_t *) &buf.base[offset]) : NULL;
    IHS_Client *client = handle->loop->data;
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
    cleanup:
    free(buf.base);
}
