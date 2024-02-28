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

#include <stdbool.h>

#include "protobuf/remoteplay.pb-c.h"
#include "protobuf/pb_utils.h"

#include "session/session_pri.h"

#include "ch_discovery.h"
#include "ch_control.h"
#include "endianness.h"
#include "ihs_buffer_ext.h"
#include "hid/manager.h"

typedef struct {
    IHS_SessionChannel base;
    IHS_TimerTask *disconnectTimerTask;
} DiscoveryChannel;

static void OnDiscoveryDeinit(IHS_SessionChannel *channel);

static void OnDiscoveryReceived(IHS_SessionChannel *channel, IHS_SessionPacket *packet);

static void OnConnectACK(IHS_SessionChannel *channel, const IHS_SessionPacket *packet);

static void OnUnconnected(IHS_SessionChannel *channel, const IHS_SessionPacket *packet);

static void OnDisconnect(IHS_SessionChannel *channel);

static void OnPingRequest(IHS_SessionChannel *channel, const IHS_SessionPacket *packet,
                          const CDiscoveryPingRequest *request);

static uint64_t DisconnectTimerRun(int runCount, void *context);

static void DisconnectTimerEnd(void *context);

static const IHS_SessionChannelClass ChannelClass = {
        .received = OnDiscoveryReceived,
        .deinit = OnDiscoveryDeinit,
        .instanceSize = sizeof(DiscoveryChannel)
};

IHS_SessionChannel *IHS_SessionChannelDiscoveryCreate(IHS_Session *session) {
    return IHS_SessionChannelCreate(&ChannelClass, session, IHS_SessionChannelTypeDiscovery,
                                    IHS_SessionChannelIdDiscovery, NULL);
}

void IHS_SessionChannelDiscoveryDisconnect(IHS_SessionChannel *channel) {
    DiscoveryChannel *discoveryCh = (DiscoveryChannel *) channel;
    discoveryCh->disconnectTimerTask = IHS_TimerTaskStart(channel->session->timers, DisconnectTimerRun,
                                                          DisconnectTimerEnd, 0, channel);
}

static void OnDiscoveryDeinit(IHS_SessionChannel *channel) {
    DiscoveryChannel *discoveryCh = (DiscoveryChannel *) channel;
    if (discoveryCh->disconnectTimerTask != NULL) {
        IHS_TimerTaskStop(discoveryCh->disconnectTimerTask);
    }
}

static void OnDiscoveryReceived(IHS_SessionChannel *channel, IHS_SessionPacket *packet) {
    switch (packet->header.type) {
        case IHS_SessionPacketTypeConnectACK:
            OnConnectACK(channel, packet);
            break;
        case IHS_SessionPacketTypeUnconnected:
            OnUnconnected(channel, packet);
            break;
        case IHS_SessionPacketTypeDisconnect:
            /* It's not possible to get packet after interruption, so we can safely call OnDisconnect. */
            OnDisconnect(channel);
            break;
        default:
            break;
    }
}

static void OnConnectACK(IHS_SessionChannel *channel, const IHS_SessionPacket *packet) {
    IHS_Session *session = channel->session;
    if (session->state.connectionId != packet->header.dstConnectionId) return;
    session->state.hostConnectionId = packet->header.srcConnectionId;
    IHS_SessionCancelRetransmission(session, IHS_SessionChannelIdDiscovery, 0, 0);

    IHS_SessionChannel *control = IHS_SessionChannelFor(session, IHS_SessionChannelIdControl);
    IHS_SessionChannelControlHandshake(control, false);
}

static void OnUnconnected(IHS_SessionChannel *channel, const IHS_SessionPacket *packet) {
    size_t offset = 0;
    EStreamDiscoveryMessage type = *IHS_BufferPointer(&packet->body);
    offset += 1;
    uint32_t messageSize;
    offset += IHS_ReadUInt32LE(IHS_BufferPointerAt(&packet->body, offset), &messageSize);
    if (type == k_EStreamDiscoveryPingRequest) {
        CDiscoveryPingRequest *request = cdiscovery_ping_request__unpack(NULL, messageSize,
                                                                         IHS_BufferPointerAt(&packet->body, offset));
        OnPingRequest(channel, packet, request);
        cdiscovery_ping_request__free_unpacked(request, NULL);
    }
}

static void OnDisconnect(IHS_SessionChannel *channel) {
    IHS_Session *session = channel->session;
    assert(session != NULL);
    IHS_BaseLock((IHS_Base *) session);
    if (session->base.interrupted) {
        IHS_BaseUnlock((IHS_Base *) session);
        return;
    }
    if (session->callbacks.session && session->callbacks.session->disconnected) {
        session->callbacks.session->disconnected(session, session->callbackContexts.session);
    }
    IHS_SessionLog(session, IHS_LogLevelInfo, "Session", "Session disconnected");
    IHS_SessionChannel *control = IHS_SessionChannelFor(session, IHS_SessionChannelIdControl);
    IHS_SessionChannelControlStopHeartbeat(control);
    IHS_HIDManagerCloseAll(session->hidManager);
    IHS_SessionInterrupt(session);
    IHS_BaseUnlock((IHS_Base *) session);
}

static void OnPingRequest(IHS_SessionChannel *channel, const IHS_SessionPacket *packet,
                          const CDiscoveryPingRequest *request) {
    CDiscoveryPingResponse response = CDISCOVERY_PING_RESPONSE__INIT;
    PROTOBUF_C_SET_VALUE(response, sequence, request->sequence);
    PROTOBUF_C_SET_VALUE(response, packet_size_received, IHS_PACKET_HEADER_SIZE + packet->body.size);
    size_t msgSize = cdiscovery_ping_response__get_packed_size(&response);

    IHS_SessionPacket outPacket;
    IHS_SessionChannelInitializePacket(channel, &outPacket, IHS_SessionPacketTypeUnconnected, true, 0);
    IHS_BufferAppendUInt8(&outPacket.body, k_EStreamDiscoveryPingResponse);
    IHS_BufferAppendUInt32LE(&outPacket.body, msgSize);
    IHS_BufferAppendMessage(&outPacket.body, (const ProtobufCMessage *) &response);

    IHS_SessionPacketPadTo(&outPacket, request->packet_size_requested);
    IHS_SessionQueuePacket(channel->session, &outPacket, false);
    IHS_SessionPacketClear(&outPacket, true);
}

static uint64_t DisconnectTimerRun(int runCount, void *context) {
    IHS_SessionChannel *channel = context;
    if (channel->session == NULL || channel->session->base.interrupted || runCount > 10) {
        return 0;
    }
    IHS_SessionPacket packet;
    IHS_SessionChannelInitializePacket(channel, &packet, IHS_SessionPacketTypeDisconnect, true, 0);
    packet.header.retransmitCount = runCount;
    IHS_SessionChannelQueuePacket(channel, &packet, false);
    IHS_SessionPacketClear(&packet, true);
    return 100;
}

static void DisconnectTimerEnd(void *context) {
    DiscoveryChannel *discoveryChannel = context;
    discoveryChannel->disconnectTimerTask = NULL;
    OnDisconnect(&discoveryChannel->base);
}