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

#pragma once

#include "ihslib/session.h"
#include "base.h"
#include "packet.h"
#include "retransmission.h"

#include "channels/channel.h"

#include "protobuf/remoteplay.pb-c.h"
#include "ihs_queue.h"

/**
 * Connection phase. Advances monotonically from Unconnected to Connected; never goes
 * backwards within a single session lifetime (a re-connect requires a fresh session).
 * Guards on the message-handling entry points use this to drop stale or replayed
 * packets that would otherwise re-trigger an earlier phase.
 *
 *   Unconnected   - IHS_SessionCreate just returned, no Connect packet sent yet.
 *   Connecting    - Connect packet sent on the discovery channel, waiting for ACK.
 *   Handshaking   - ConnectACK received, ClientHandshake sent, waiting for ServerHandshake.
 *   Authenticating- ServerHandshake received, AuthenticationRequest sent, waiting for response.
 *   Negotiating   - AuthenticationResponse = Succeeded, waiting for NegotiationInit / sending SetConfig.
 *   Connected     - NegotiationComplete sent, the stream is live.
 */
typedef enum IHS_SessionConnectionState {
    IHS_SessionConnectionStateUnconnected,
    IHS_SessionConnectionStateConnecting,
    IHS_SessionConnectionStateHandshaking,
    IHS_SessionConnectionStateAuthenticating,
    IHS_SessionConnectionStateNegotiating,
    IHS_SessionConnectionStateConnected,
} IHS_SessionConnectionState;

typedef struct IHS_SessionState {
    IHS_SessionConnectionState connectionState;
    int mtu;
    uint8_t connectionId;
    uint8_t hostConnectionId;
    /**
     * Negotiated stream-enable flags from CSetStreamingClientConfig. Default true at
     * session creation since the streaming request asked for all three; the server
     * confirms (or vetoes) via OnSetClientConfig. Mirrors Steam's BStreamingInput /
     * BStreamingAudio / BStreamingVideo accessors reading from
     * clientConfig.enable_*_streaming.
     */
    bool streamingInput;
    bool streamingAudio;
    bool streamingVideo;
} IHS_SessionState;

struct IHS_Session {
    IHS_Base base;
    IHS_SessionInfo info;
    IHS_SessionState state;
    uint8_t numChannels;
    IHS_SessionChannel *channels[16];
    IHS_Thread *sendThread;
    IHS_Cond *sendQueueCond;
    IHS_Mutex *sendQueueMutex;
    IHS_Queue *sendQueue;
    IHS_Timer *timers;
    IHS_SessionRetransmission retransmission;
    IHS_HIDManager *hidManager;
    struct {
        const IHS_StreamSessionCallbacks *session;
        const IHS_StreamAudioCallbacks *audio;
        const IHS_StreamVideoCallbacks *video;
        const IHS_StreamInputCallbacks *input;
        const IHS_StreamMicrophoneCallbacks *microphone;
    } callbacks;
    struct {
        void *session;
        void *audio;
        void *video;
        void *input;
        void *microphone;
    } callbackContexts;
};

#define IHS_SessionLog(session, level, tag, ...) IHS_BaseLog((IHS_Base*) (session), (level), (tag), __VA_ARGS__)

void IHS_SessionInterrupt(IHS_Session *session);

bool IHS_SessionSendPacket(IHS_Session *session, IHS_SessionPacket *packet);

/**
 * Add packet to send queue
 * @param session Session instance
 * @param packet Packet
 * @param retransmit If true, the packet will be retransmitted 10 times before cancellation
 * @return
 */
bool IHS_SessionQueuePacket(IHS_Session *session, IHS_SessionPacket *packet, bool retransmit);

bool IHS_SessionSendControlMessage(IHS_Session *session, EStreamControlMessage type, const ProtobufCMessage *message);

/**
 * @return true if the session-state input flag is set. Outbound input/HID send sites should
 *   early-return false when this is false. Mirrors Steam's BStreamingInput().
 */
bool IHS_SessionInputEnabled(IHS_Session *session);

bool IHS_SessionCancelRetransmission(IHS_Session *session, IHS_SessionChannelId channelId, uint16_t packetId,
                                     uint16_t fragmentId);
