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

#include <stdlib.h>
#include <string.h>

#include "crypto.h"
#include "session/frame.h"
#include "session/window.h"
#include "ch_discovery.h"

#include "session/session_pri.h"
#include "protobuf/pb_utils.h"

#include "ihs_buffer_ext.h"

static bool IsMessageEncrypted(EStreamControlMessage type);

static size_t EncryptedMessageCapacity(size_t plainSize);

static void OnControlInit(IHS_SessionChannel *channel, const void *data);

static void OnControlDeinit(IHS_SessionChannel *channel);

static void OnControlReceived(IHS_SessionChannel *channel, IHS_SessionPacket *packet);

static void OnControlMessageReceived(IHS_SessionChannel *channel, EStreamControlMessage type, IHS_Buffer *payload,
                                     const IHS_SessionPacketHeader *header);

static void OnServerHandshake(IHS_SessionChannel *channel, const CServerHandshakeMsg *message);

static void OnSetClientConfig(IHS_SessionChannel *channel, const CSetStreamingClientConfig *message);

static void OnSetSpectatorMode(IHS_SessionChannel *channel, const CSetSpectatorModeMsg *message);

static void OnSetQoS(IHS_SessionChannel *channel, const CSetQoSMsg *message);

static const char *ControlMessageTypeName(EStreamControlMessage type);

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
    const ProtobufCEnumValue *value = protobuf_c_enum_descriptor_get_value(&estream_control_message__descriptor,
                                                                           type);
    enum IHS_LogLevel logLevel;
    switch (type) {
        case k_EStreamControlRemoteHID:
            logLevel = IHS_LogLevelVerbose;
            break;
        default:
            logLevel = IHS_LogLevelDebug;
            break;
    }
    IHS_SessionFrame frame;
    IHS_SessionChannelInitializeFrame(channel, &frame, IHS_SessionPacketTypeReliable, true, packetId);
    IHS_SessionLog(channel->session, logLevel, "Control", "Send control message: %s, id=%u", value->name,
                   frame.header.packetId);
    IHS_BufferAppendUInt8(&frame.body, type);
    if (IsMessageEncrypted(type)) {
        size_t cipherSize = EncryptedMessageCapacity(messageCapacity);
        uint8_t *serialized = calloc(1, messageCapacity);
        size_t serializedLen = protobuf_c_message_pack(message, serialized);
        uint8_t *cipher = IHS_BufferPointerForAppend(&frame.body, cipherSize);
        if (IHS_SessionFrameEncrypt(channel->session, serialized, serializedLen, cipher, &cipherSize,
                                    control->sendEncryptSequence++) != 0) {
            free(serialized);
            IHS_SessionFrameClear(&frame, true);
            IHS_SessionLog(channel->session, IHS_LogLevelError, "Control", "Failed to encrypt payload\n");
            IHS_SessionDisconnect(channel->session);
            return false;
        }
        free(serialized);
        frame.body.size += cipherSize;
    } else {
        IHS_BufferAppendMessage(&frame.body, message);
    }
    ret = IHS_SessionChannelQueueFrame(channel, &frame, true);
    IHS_SessionFrameClear(&frame, true);
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

static void OnControlReceived(IHS_SessionChannel *channel, IHS_SessionPacket *packet) {
    IHS_SessionChannelControl *control = (IHS_SessionChannelControl *) channel;
    IHS_SessionPacketsWindow *window = control->framePacketWindow;
    switch (packet->header.type) {
        case IHS_SessionPacketTypeReliable:
        case IHS_SessionPacketTypeReliableFrag:
            if (!IHS_SessionPacketsWindowAdd(window, packet)) {
                IHS_SessionLog(channel->session, IHS_LogLevelError, "Control", "Frames window overflow");
                IHS_SessionDisconnect(channel->session);
            }
            IHS_SessionChannelPacketAck(channel, packet->header.packetId, true);
            break;
        case IHS_SessionPacketTypeACK:
        case IHS_SessionPacketTypeNACK:
            // Stop retransmit of the packet
            break;
        default:
            // Other items should not come here
            IHS_SessionLog(channel->session, IHS_LogLevelError, "Control", "Unrecognized packet %u\n",
                           packet->header.type);
            IHS_SessionDisconnect(channel->session);
            break;
    }
    IHS_SessionFrame frame;
    IHS_BufferInit(&frame.body, 1024, 1024 * 1024);

    for (; IHS_SessionPacketsWindowPoll(window, &frame); IHS_SessionPacketsWindowReleaseFrame(&frame)) {
        EStreamControlMessage type = *IHS_BufferPointer(&frame.body);
        IHS_BufferOffsetBy(&frame.body, 1);
        if (IsMessageEncrypted(type)) {
            IHS_Buffer plain;
            IHS_BufferInit(&plain, 1024, 1024 * 1024);
            uint64_t expectSequence = control->recvEncryptSequence++, actualSequence;
            switch (IHS_SessionFrameDecrypt(channel->session, &frame.body, &plain, expectSequence, &actualSequence)) {
                case IHS_SessionPacketResultOK: {
                    OnControlMessageReceived(channel, type, &plain, &frame.header);
                    break;
                }
                case IHS_SessionFrameDecryptHashMismatch:
                case IHS_SessionFrameDecryptOldSequence: {
                    // Ignore this packet
                    break;
                }
                case IHS_SessionFrameDecryptSequenceMismatch: {
                    IHS_SessionLog(channel->session, IHS_LogLevelWarn, "Control",
                                   "Mismatched message sequence %llu (expect %llu). id=%d, retransmit=%d, type=%s",
                                   actualSequence, expectSequence, frame.header.packetId, frame.header.retransmitCount,
                                   ControlMessageTypeName(type));
                    control->recvEncryptSequence = actualSequence + 1;
                    break;
                }
                case IHS_SessionFrameDecryptFailed: {
                    IHS_SessionLog(channel->session, IHS_LogLevelWarn, "Control",
                                   "Failed to decrypt control message. id=%d, retransmit=%d, type=%s",
                                   frame.header.packetId, frame.header.retransmitCount, ControlMessageTypeName(type));
                    break;
                }
            }
            IHS_BufferClear(&plain, true);
        } else {
            OnControlMessageReceived(channel, type, &frame.body, &frame.header);
        }
    }

    IHS_BufferClear(&frame.body, true);
}

static void OnControlMessageReceived(IHS_SessionChannel *channel, EStreamControlMessage type, IHS_Buffer *payload,
                                     const IHS_SessionPacketHeader *header) {
    switch (type) {
        case k_EStreamControlServerHandshake: {
            CServerHandshakeMsg *message = IHS_UNPACK_BUFFER(cserver_handshake_msg__unpack, payload);
            OnServerHandshake(channel, message);
            cserver_handshake_msg__free_unpacked(message, NULL);
            break;
        }
        case k_EStreamControlAuthenticationResponse: {
            IHS_SessionChannelControlOnAuthentication(channel, type, payload, header);
            break;
        }
        case k_EStreamControlNegotiationInit:
        case k_EStreamControlNegotiationSetConfig: {
            IHS_SessionChannelControlOnNegotiation(channel, type, payload, header);
            break;
        }
        case k_EStreamControlSetStreamingClientConfig: {
            CSetStreamingClientConfig *message = IHS_UNPACK_BUFFER(cset_streaming_client_config__unpack, payload);
            OnSetClientConfig(channel, message);
            cset_streaming_client_config__free_unpacked(message, NULL);
            break;
        }
        case k_EStreamControlSetSpectatorMode: {
            CSetSpectatorModeMsg *message = IHS_UNPACK_BUFFER(cset_spectator_mode_msg__unpack, payload);
            OnSetSpectatorMode(channel, message);
            cset_spectator_mode_msg__free_unpacked(message, NULL);
            break;
        }
        case k_EStreamControlStartAudioData:
        case k_EStreamControlStopAudioData: {
            IHS_SessionChannelControlOnAudio(channel, type, payload, header);
            break;
        }
        case k_EStreamControlStartVideoData:
        case k_EStreamControlStopVideoData:
        case k_EStreamControlVideoEncoderInfo:
        case k_EStreamControlSetCaptureSize:
        case k_EStreamControlSetTargetFramerate: {
            IHS_SessionChannelControlOnVideo(channel, type, payload, header);
            break;
        }
        case k_EStreamControlSetQoS: {
            CSetQoSMsg *message = IHS_UNPACK_BUFFER(cset_qo_smsg__unpack, payload);
            OnSetQoS(channel, message);
            cset_qo_smsg__free_unpacked(message, NULL);
            break;
        }
        case k_EStreamControlSetTargetBitrate:
            break;
        case k_EStreamControlShowCursor:
        case k_EStreamControlHideCursor:
        case k_EStreamControlSetCursor:
        case k_EStreamControlGetCursorImage:
        case k_EStreamControlSetCursorImage:
        case k_EStreamControlDeleteCursor: {
            IHS_SessionChannelControlOnCursor(channel, type, payload, header);
            break;
        }
        case k_EStreamControlSetKeymap: {
            CSetKeymapMsg *message = IHS_UNPACK_BUFFER(cset_keymap_msg__unpack, payload);
            cset_keymap_msg__free_unpacked(message, NULL);
            break;
        }
        case k_EStreamControlSetTitle: {
            CSetTitleMsg *message = IHS_UNPACK_BUFFER(cset_title_msg__unpack, payload);
            IHS_SessionLog(channel->session, IHS_LogLevelInfo, "Control", "Set title: %s", message->text);
            cset_title_msg__free_unpacked(message, NULL);
            break;
        }
        case k_EStreamControlSetIcon:
        case k_EStreamControlSetActivity:
            break;
        case k_EStreamControlRemoteHID: {
            CRemoteHIDMsg *message = IHS_UNPACK_BUFFER(cremote_hidmsg__unpack, payload);
            if (message->has_data) {
                CHIDMessageToRemote *hid = chidmessage_to_remote__unpack(NULL, message->data.len, message->data.data);
                IHS_SessionChannelControlOnHIDMsg(channel, hid);
                chidmessage_to_remote__free_unpacked(hid, NULL);
            }
            cremote_hidmsg__free_unpacked(message, NULL);
            break;
        }
        case k_EStreamControlControllerConfigMsg: {
            CControllerConfigMsg *message = IHS_UNPACK_BUFFER(ccontroller_config_msg__unpack, payload);
            ccontroller_config_msg__free_unpacked(message, NULL);
            break;
        }
        case k_EStreamControlControllerPersonalizationUpdate: {
            CControllerPersonalizationUpdateMsg *message = IHS_UNPACK_BUFFER(
                    ccontroller_personalization_update_msg__unpack, payload);
            ccontroller_personalization_update_msg__free_unpacked(message, NULL);
            break;
        }
        default: {
            IHS_SessionLog(channel->session, IHS_LogLevelInfo, "Control", "Unhandled control message: %s",
                           ControlMessageTypeName(type));
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

static void OnSetClientConfig(IHS_SessionChannel *channel, const CSetStreamingClientConfig *message) {
    const CStreamingClientConfig *config = message->config;
    IHS_SessionLog(channel->session, IHS_LogLevelDebug, "Control", "Set client config. enable_video_hevc=%u",
                   config->enable_video_hevc);

}

static void OnSetSpectatorMode(IHS_SessionChannel *channel, const CSetSpectatorModeMsg *message) {
    IHS_SessionLog(channel->session, IHS_LogLevelDebug, "Control", "Set client config. spectator_mode=%u",
                   message->enabled);
}

static void OnSetQoS(IHS_SessionChannel *channel, const CSetQoSMsg *message) {
    IHS_SessionLog(channel->session, IHS_LogLevelDebug, "Control", "Set QoS config. use_qos=%u",
                   message->use_qos);
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

static const char *ControlMessageTypeName(EStreamControlMessage type) {
    const ProtobufCEnumValue *value = protobuf_c_enum_descriptor_get_value(&estream_control_message__descriptor,
                                                                           type);
    return value ? value->name : "unknown";
}