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
#include "ch_data.h"

#include "protobuf/discovery.pb-c.h"
#include "client/client_pri.h"
#include "protobuf/pb_utils.h"

static void OnControlInit(IHS_SessionChannel *channel, const void *data);

static void OnControlDeinit(IHS_SessionChannel *channel);

static void OnControlReceived(IHS_SessionChannel *channel, const IHS_SessionPacket *packet);

static void OnControlMessageReceived(IHS_SessionChannel *channel, EStreamControlMessage type, const uint8_t *payload,
                                     size_t payloadLen, const IHS_SessionPacketHeader *header);

static bool IsMessageEncrypted(EStreamControlMessage type);

static size_t EncryptedMessageCapacity(size_t plainSize);

static void OnServerHandshake(IHS_SessionChannel *channel, const CServerHandshakeMsg *message);

static const IHS_SessionChannelClass ChannelClass = {
        .init = OnControlInit,
        .deinit = OnControlDeinit,
        .received = OnControlReceived,
        .instanceSize = sizeof(IHS_SessionChannelControl)
};

IHS_SessionChannel *IHS_SessionChannelControlCreate(IHS_Session *session) {
    return IHS_SessionChannelCreate(&ChannelClass, session, IHS_SessionChannelTypeControl, IHS_SessionChannelIdControl,
                                    NULL);
}

bool IHS_SessionChannelControlSend(IHS_SessionChannel *channel, EStreamControlMessage type,
                                   const ProtobufCMessage *message, int32_t packetId) {
    assert(channel->id == IHS_SessionChannelIdControl);
    IHS_SessionChannelControl *control = (IHS_SessionChannelControl *) channel;
    size_t messageCapacity = protobuf_c_message_get_packed_size(message);
    bool ret;
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
            return false;
        }
        free(serialized);

        size_t payloadSize = 1 + cipherSize;
        ret = IHS_SessionChannelSendBytes(channel, IHS_SessionPacketTypeReliable, true, packetId,
                                          payload, payloadSize, 0);
        free(payload);
    } else {
        uint8_t *payload = malloc(1 + messageCapacity);
        payload[0] = type;
        size_t payloadSize = 1 + protobuf_c_message_pack(message, &payload[1]);
        ret = IHS_SessionChannelSendBytes(channel, IHS_SessionPacketTypeReliable, true, packetId,
                                          payload, payloadSize, 0);
        free(payload);
    }
    return ret;
}

void IHS_SessionChannelControlHandshake(IHS_SessionChannel *channel, bool networkTest) {
    CClientHandshakeMsg message = CCLIENT_HANDSHAKE_MSG__INIT;
    CStreamingClientHandshakeInfo handshakeInfo = CSTREAMING_CLIENT_HANDSHAKE_INFO__INIT;
    if (networkTest) {
        PROTOBUF_C_SET_VALUE(handshakeInfo, network_test, true);
    }
    message.info = &handshakeInfo;
    IHS_SessionChannelControlSend(channel, k_EStreamControlClientHandshake, (const ProtobufCMessage *) &message,
                                  IHS_PACKET_ID_NEXT);
}

static void OnControlInit(IHS_SessionChannel *channel, const void *data) {
    IHS_UNUSED(data);
    IHS_SessionChannelControl *control = (IHS_SessionChannelControl *) channel;
    control->framePacketWindow = IHS_SessionPacketsWindowCreate(128);
}

static void OnControlDeinit(IHS_SessionChannel *channel) {
    IHS_SessionChannelControl *control = (IHS_SessionChannelControl *) channel;
    IHS_SessionPacketsWindowDestroy(control->framePacketWindow);
}

static void OnControlReceived(IHS_SessionChannel *channel, const IHS_SessionPacket *packet) {
    IHS_SessionChannelControl *control = (IHS_SessionChannelControl *) channel;
    IHS_SessionPacketsWindow *window = control->framePacketWindow;
    switch (packet->header.type) {
        case IHS_SessionPacketTypeReliable:
        case IHS_SessionPacketTypeReliableFrag:
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
            if (IHS_SessionFrameDecrypt(channel->session, &frame.body[1], messageLen,
                                        plain, &messageLen, control->recvEncryptSequence) != 0) {
                free(plain);
                fprintf(stderr, "Failed to decrypt message id=%d, retransmit=%d, type=%d\n",
                        frame.header.packetId, frame.header.retransmitCount, type);
                IHS_SessionDisconnect(channel->session);
                continue;
            }
            control->recvEncryptSequence++;
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
        case k_EStreamControlAuthenticationResponse:
            IHS_SessionChannelControlOnAuthentication(channel, type, payload, payloadLen, header);
            break;
        case k_EStreamControlNegotiationInit:
        case k_EStreamControlNegotiationSetConfig: {
            IHS_SessionChannelControlOnNegotiation(channel, type, payload, payloadLen, header);
            break;
        }
        case k_EStreamControlStartAudioData:
        case k_EStreamControlStopAudioData: {
            IHS_SessionChannelControlOnAudio(channel, type, payload, payloadLen, header);
            break;
        }
        case k_EStreamControlStartVideoData:
        case k_EStreamControlStopVideoData:
        case k_EStreamControlVideoEncoderInfo:
        case k_EStreamControlSetCaptureSize:
        case k_EStreamControlSetTargetFramerate: {
            IHS_SessionChannelControlOnVideo(channel, type, payload, payloadLen, header);
            break;
        }
        case k_EStreamControlSetQoS:
        case k_EStreamControlSetTargetBitrate:
        case k_EStreamControlShowCursor:
        case k_EStreamControlSetCursor: {
            break;
        }
        default: {
            const ProtobufCEnumValue *value = protobuf_c_enum_descriptor_get_value(&estream_control_message__descriptor,
                                                                                   type);
            fprintf(stderr, "Unhandled control message: %s\n", value ? value->name : "unknown");
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
    IHS_SessionChannelControlRequestAuthentication(channel);
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