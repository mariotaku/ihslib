#include "ihslib/client.h"

#include <stdint.h>
#include <malloc.h>
#include <memory.h>

#include <uv.h>

#include "protobuf/discovery.pb-c.h"
#include "endianness.h"
#include "client_pri.h"
#include "ihslib/crypto.h"


static const unsigned char PACKET_MAGIC[8] = {0xff, 0xff, 0xff, 0xff, 0x21, 0x4c, 0x5f, 0xa0};

static void IHS_ClientThread(IHS_Client *client);

static uv_buf_t IHS_BufferAlloc(uv_handle_t *handle, size_t suggested_size);

static void IHS_Send(IHS_Client *client, const char *ip, ERemoteClientBroadcastMsg type, ProtobufCMessage *message);

static void IHS_SendCallback(uv_udp_send_t *req, int status);

static void IHS_RecvCallback(uv_udp_t *handle, ssize_t nread, uv_buf_t buf,
                             struct sockaddr *addr, unsigned flags);

static const ProtobufCMessageDescriptor *IHS_MessageDescriptors[k_ERemoteDeviceStreamingProgress + 1] = {
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

IHS_Client *IHS_ClientCreate(uint64_t deviceId, const uint8_t *secretKey, const char *deviceName) {
    IHS_Client *client = malloc(sizeof(IHS_Client));
    client->deviceId = deviceId;
    memcpy(client->secretKey, secretKey, 32);
    strncpy(client->deviceName, deviceName ? deviceName : "IHSLib", sizeof(client->deviceName));
    client->privCallbacks.discovery = IHS_ClientPriDiscoveryCallback;
    client->privCallbacks.authorization = IHS_ClientPriAuthorizationCallback;
    client->loop = uv_loop_new();
    uv_mutex_init(&client->mutex);
    client->loop->data = client;
    uv_udp_init(client->loop, &client->udp);
    uv_udp_bind(&client->udp, uv_ip4_addr("0.0.0.0", 0), 0);
    uv_udp_set_broadcast(&client->udp, 1);
    uv_udp_recv_start(&client->udp, IHS_BufferAlloc, IHS_RecvCallback);
    uv_thread_create(&client->workerThread, (void (*)(void *)) IHS_ClientThread, client);
    return client;
}

void IHS_ClientStop(IHS_Client *client) {
    uv_stop(client->loop);
}

void IHS_ClientDestroy(IHS_Client *client) {
    uv_thread_join(&client->workerThread);
    uv_mutex_destroy(&client->mutex);
    uv_loop_delete(client->loop);
    free(client);
}

void IHS_ClientSetCallbacks(IHS_Client *client, const IHS_ClientCallbacks *callbacks) {
    client->callbacks = *callbacks;
}

void IHS_ClientLock(IHS_Client *client) {
    uv_mutex_lock(&client->mutex);
}

void IHS_ClientUnlock(IHS_Client *client) {
    uv_mutex_unlock(&client->mutex);
}

static void IHS_ClientThread(IHS_Client *client) {
    int result = uv_run(client->loop, UV_RUN_DEFAULT);
    printf("Loop ended with result %d\n", result);
}

static uv_buf_t IHS_BufferAlloc(uv_handle_t *handle, size_t suggested_size) {
    (void) handle;
    return uv_buf_init(malloc(suggested_size), suggested_size);
}

void IHS_ClientPriDeviceToken(IHS_Client *client, uint8_t *token, size_t *tokenLen) {
    uint8_t in[8];
    IHS_WriteUInt64LE(in, client->deviceId);
    IHS_CryptoSymmetricEncrypt(in, 8, client->secretKey, sizeof(client->secretKey), token, tokenLen);
}

void IHS_ClientPriSend(IHS_Client *client, const char *ip, ERemoteClientBroadcastMsg type, ProtobufCMessage *message) {
    CMsgRemoteClientBroadcastHeader header;
    cmsg_remote_client_broadcast_header__init(&header);
    header.has_client_id = 1;
    header.client_id = client->deviceId;
    header.has_msg_type = 1;
    header.msg_type = type;
    size_t header_size = cmsg_remote_client_broadcast_header__get_packed_size(&header);
    size_t payload_size = message ? protobuf_c_message_get_packed_size(message) : 0;

    uint8_t pkt_data[1024];
    ProtobufCBufferSimple buf = PROTOBUF_C_BUFFER_SIMPLE_INIT(pkt_data);
    protobuf_c_buffer_simple_append((ProtobufCBuffer *) &buf, sizeof(PACKET_MAGIC), PACKET_MAGIC);
    IHS_AppendUInt32LEToBuffer(header_size, &buf);
    cmsg_remote_client_broadcast_header__pack_to_buffer(&header, (ProtobufCBuffer *) &buf);
    IHS_AppendUInt32LEToBuffer(payload_size, &buf);
    if (message) {
        protobuf_c_message_pack_to_buffer(message, (ProtobufCBuffer *) &buf);
    }

    uv_buf_t uvbuf = uv_buf_init(malloc(buf.len), buf.len);
    memcpy(uvbuf.base, buf.data, buf.len);
    struct sockaddr_in send_addr = uv_ip4_addr(ip, 27036);
    uv_udp_send_t *req = malloc(sizeof(uv_udp_send_t));
    uv_udp_send(req, &client->udp, &uvbuf, 1, send_addr, IHS_SendCallback);
}

static void IHS_SendCallback(uv_udp_send_t *req, int status) {
    if (status != 0) {
        printf("Error: %s\n", strerror(status));
    }
    free(req->bufsml[0].base);
    free(req);
}

static void IHS_RecvCallback(uv_udp_t *handle, ssize_t nread, uv_buf_t buf,
                             struct sockaddr *addr, unsigned flags) {
    if (!nread) return;
    size_t offset = 0;
    if (memcmp(&buf.base[offset], PACKET_MAGIC, sizeof(PACKET_MAGIC)) != 0) {
        fprintf(stderr, "Unrecognized packet!\n");
        return;
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
    const ProtobufCMessageDescriptor *descriptor = IHS_MessageDescriptors[type];
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
            client->privCallbacks.session(client, address, header, message);
            break;
        default:
            break;
    }
    cmsg_remote_client_broadcast_header__free_unpacked(header, NULL);
    protobuf_c_message_free_unpacked(message, NULL);
}
