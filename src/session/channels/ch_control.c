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

#include "ch_control.h"

#include <malloc.h>
#include <string.h>

#include "crypto.h"
#include "session/frame.h"
#include "ch_discovery.h"
#include "protobuf/discovery.pb-c.h"

typedef struct ControlChannel {
    IHS_SessionChannel base;
    uint64_t sendEncryptSequence;
    uint64_t recvEncryptSequence;
    IHS_SessionPacketsWindow framePacketWindow;
} ControlChannel;

static void OnControlInit(IHS_SessionChannel *channel);

static void OnControlReceived(IHS_SessionChannel *channel, const IHS_SessionPacket *packet);

static void OnControlMessageReceived(IHS_SessionChannel *channel, EStreamControlMessage type, const uint8_t *payload,
                                     size_t payloadLen, const IHS_SessionPacketHeader *header);

static bool IsMessageEncrypted(EStreamControlMessage type);

static size_t EncryptedMessageCapacity(size_t plainSize);

static void OnServerHandshake(IHS_SessionChannel *channel, const CServerHandshakeMsg *message);

static void OnAuthenticationResponse(IHS_SessionChannel *channel, const CAuthenticationResponseMsg *message);

static void RequestAuthentication(IHS_SessionChannel *channel);

static const IHS_SessionChannelClass Functions = {
        .init = OnControlInit,
        .onReceived = OnControlReceived,
        .instanceSize = sizeof(ControlChannel)
};

IHS_SessionChannel *IHS_SessionChannelControlCreate(IHS_Session *session) {
    return IHS_SessionChannelCreate(&Functions, session, IHS_SessionChannelIdControl);
}

void IHS_SessionChannelControlSend(IHS_SessionChannel *channel, EStreamControlMessage type,
                                   const ProtobufCMessage *message, int32_t packetId) {
    assert(channel->id == IHS_SessionChannelIdControl);
    ControlChannel *control = (ControlChannel *) channel;
    size_t messageCapacity = protobuf_c_message_get_packed_size(message);
    if (IsMessageEncrypted(type)) {
        size_t cipherSize = EncryptedMessageCapacity(messageCapacity);
        uint8_t *payload = malloc(1 + cipherSize);
        payload[0] = type;
        uint8_t *serialized = calloc(1, messageCapacity);
        size_t serializedLen = protobuf_c_message_pack(message, serialized);
        if (IHS_SessionFrameEncrypt(channel->session, serialized, serializedLen, &payload[1], &cipherSize,
                                    control->sendEncryptSequence++) != 0) {
            free(serialized);
            free(payload);
            fprintf(stderr, "Failed to encrypt payload\n");
            IHS_SessionDisconnect(channel->session);
            return;
        }
        free(serialized);

        size_t payloadSize = 1 + cipherSize;
        IHS_SessionChannelSendBytes(channel, IHS_SessionPacketTypeReliable, true, packetId, payload,
                                    payloadSize, 0);
        free(payload);
    } else {
        uint8_t *payload = malloc(1 + messageCapacity);
        payload[0] = type;
        size_t payloadSize = 1 + protobuf_c_message_pack(message, &payload[1]);
        IHS_SessionChannelSendBytes(channel, IHS_SessionPacketTypeReliable, true, packetId, payload,
                                    payloadSize, 0);
        free(payload);
    }
}

void IHS_SessionChannelControlHandshake(IHS_SessionChannel *channel, bool networkTest) {
    CClientHandshakeMsg message = CCLIENT_HANDSHAKE_MSG__INIT;
    CStreamingClientHandshakeInfo handshakeInfo = CSTREAMING_CLIENT_HANDSHAKE_INFO__INIT;
    handshakeInfo.has_network_test = true;
    handshakeInfo.network_test = networkTest;
    message.info = &handshakeInfo;
    IHS_SessionChannelControlSend(channel, k_EStreamControlClientHandshake, (const ProtobufCMessage *) &message,
                                  IHS_PACKET_ID_NEXT);
}

static void OnControlInit(IHS_SessionChannel *channel) {
    ControlChannel *control = (ControlChannel *) channel;
    IHS_SessionPacketsWindowInit(&control->framePacketWindow, 128);
}

static void OnControlReceived(IHS_SessionChannel *channel, const IHS_SessionPacket *packet) {
    ControlChannel *control = (ControlChannel *) channel;
    IHS_SessionPacketsWindow *window = &control->framePacketWindow;
    switch (packet->header.type) {
        case IHS_SessionPacketTypeReliable:
        case IHS_SessionPacketTypeReliableFrag:
            if (packet->header.hasCrc && !packet->crcOK) {
                IHS_SessionChannelPacketAck(channel, packet->header.packetId, false);
                break;
            }
            if (!IHS_SessionPacketsWindowAdd(window, packet)) {
                fprintf(stderr, "Packets window overflow\n");
                IHS_SessionDisconnect(channel->session);
            }
            IHS_SessionChannelPacketAck(channel, packet->header.packetId, true);
            break;
        case IHS_SessionPacketTypeACK:
        case IHS_SessionPacketTypeNACK:
            // Stop retransmit of the packet
            break;
        default:
            // Other packets should not come here
            fprintf(stderr, "Unrecognized packet %u in Control Channel\n", packet->header.type);
            IHS_SessionDisconnect(channel->session);
            break;
    }
    IHS_SessionFrame frame;
    for (; IHS_SessionPacketsWindowPoll(window, &frame); IHS_SessionPacketsWindowReleaseFrame(&frame)) {
        EStreamControlMessage type = frame.body[0];
        size_t messageLen = frame.bodyLen - 1;
        if (IsMessageEncrypted(type)) {
            uint8_t *plain = malloc(messageLen);
            uint64_t expectedSequence = control->recvEncryptSequence++;
            if (IHS_SessionFrameDecrypt(channel->session, &frame.body[1], messageLen,
                                        plain, &messageLen, expectedSequence) != 0) {
                free(plain);
                fprintf(stderr, "Failed to decrypt message\n");
                IHS_SessionDisconnect(channel->session);
                continue;
            }
            OnControlMessageReceived(channel, type, plain, messageLen, &frame.header);
            // There is no way that this is a dangling pointer
#pragma clang diagnostic push
#pragma ide diagnostic ignored "DanglingPointer"
            free(plain);
#pragma clang diagnostic pop
        } else {
            OnControlMessageReceived(channel, type, &frame.body[1], messageLen, &frame.header);
        }
    }
}

static void OnControlMessageReceived(IHS_SessionChannel *channel, EStreamControlMessage type, const uint8_t *payload,
                                     size_t payloadLen, const IHS_SessionPacketHeader *header) {
    switch (type) {
        case k_EStreamControlServerHandshake: {
            CServerHandshakeMsg *message = cserver_handshake_msg__unpack(NULL, payloadLen, payload);
            OnServerHandshake(channel, message);
            cserver_handshake_msg__free_unpacked(message, NULL);
            break;
        }
        case k_EStreamControlAuthenticationResponse: {
            CAuthenticationResponseMsg *message = cauthentication_response_msg__unpack(NULL, payloadLen, payload);
            OnAuthenticationResponse(channel, message);
            cauthentication_response_msg__free_unpacked(message, NULL);
            break;
        }
        case k_EStreamControlNegotiationInit:
        case k_EStreamControlNegotiationSetConfig: {
            IHS_SessionChannelControlOnNegotiation(channel, type, payload, payloadLen, header);
            break;
        }
        default: {
            fprintf(stderr, "Unhandled control message %u\n", type);
            IHS_SessionDisconnect(channel->session);
            break;
        }
    }
}

static void OnServerHandshake(IHS_SessionChannel *channel, const CServerHandshakeMsg *message) {
    if (message->info->has_mtu) {
        channel->session->state.mtu = message->info->mtu;
    } else {
        channel->session->state.mtu = 1500;
    }
    RequestAuthentication(channel);
}

static void OnAuthenticationResponse(IHS_SessionChannel *channel, const CAuthenticationResponseMsg *message) {
    if (!message->has_result || message->result != CAUTHENTICATION_RESPONSE_MSG__AUTHENTICATION_RESULT__SUCCEEDED) {
        fprintf(stderr, "Failed to authenticate\n");
        IHS_SessionDisconnect(channel->session);
        return;
    }
    printf("Authenticated\n");
}


static void RequestAuthentication(IHS_SessionChannel *channel) {
    CAuthenticationRequestMsg request = CAUTHENTICATION_REQUEST_MSG__INIT;
    request.has_version = true;
    request.version = k_EStreamVersionCurrent;
    request.has_token = true;
    static const unsigned char plain[] = {'S', 't', 'e', 'a', 'm', ' ', 'I', 'n',
                                          '-', 'H', 'o', 'm', 'e', ' ', 'S', 't',
                                          'r', 'e', 'a', 'm', 'i', 'n', 'g'};
    uint8_t token[32];
    request.token.data = token;
    request.token.len = sizeof(token);
    if (IHS_SessionFrameHMACSHA256(channel->session, plain, sizeof(plain), token, &request.token.len) != 0) {
        IHS_SessionDisconnect(channel->session);
        return;
    }
    IHS_SessionChannelControlSend(channel, k_EStreamControlAuthenticationRequest, (const ProtobufCMessage *) &request,
                                  IHS_PACKET_ID_NEXT);
}

static bool IsMessageEncrypted(EStreamControlMessage type) {
    switch (type) {
        case k_EStreamControlClientHandshake:
        case k_EStreamControlServerHandshake:
        case k_EStreamControlAuthenticationRequest:
        case k_EStreamControlAuthenticationResponse:
            return false;
        default:
            return true;
    }
}

static size_t EncryptedMessageCapacity(size_t plainSize) {
    /* iv + pkcs7pad(sequence + plain) */
    return 16 + ((plainSize + sizeof(uint64_t)) / IHS_CRYPTO_AES_BLOCK_SIZE + 1) * IHS_CRYPTO_AES_BLOCK_SIZE;
}