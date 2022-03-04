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

#include <malloc.h>
#include <memory.h>
#include <assert.h>

#include "channel.h"
#include "session/session_pri.h"
#include "endianness.h"


IHS_SessionChannel *IHS_SessionChannelCreate(const IHS_SessionChannelClass *cls, IHS_Session *session,
                                             IHS_SessionChannelId id) {
    assert(cls->instanceSize >= sizeof(IHS_SessionChannel));
    IHS_SessionChannel *channel = malloc(cls->instanceSize);
    memset(channel, 0, sizeof(IHS_SessionChannel));
    channel->cls = *cls;
    channel->session = session;
    channel->id = id;
    if (cls->init) {
        cls->init(channel);
    }
    return channel;
}

void IHS_SessionChannelDestroy(IHS_SessionChannel *channel) {
    if (channel->cls.deinit) {
        channel->cls.deinit(channel);
    }
    free(channel);
}

IHS_SessionChannel *IHS_SessionChannelFor(IHS_Session *session, IHS_SessionChannelId channelId) {
    for (int i = 0; i < session->numChannels; ++i) {
        IHS_SessionChannel *channel = session->channels[i];
        if (channel->id == channelId) return channel;
    }
    return NULL;
}

void IHS_SessionChannelReceivedPacket(IHS_SessionChannel *channel, const IHS_SessionPacket *packet) {
    channel->cls.onReceived(channel, packet);
}

void IHS_SessionChannelReceivedPacketBase(IHS_SessionChannel *channel, const IHS_SessionPacket *packet) {
    printf("Received packet(type=%d, channel=%d)\n", packet->header.type, packet->header.channelId);
}

uint16_t IHS_SessionChannelNextPacketId(IHS_SessionChannel *channel) {
    return channel->nextPacketId++;
}

void IHS_SessionChannelSendBytes(IHS_SessionChannel *channel, IHS_SessionPacketType type, bool hasCrc, int32_t packetId,
                                 const uint8_t *body, size_t bodyLen, size_t padTo) {
    IHS_Session *session = channel->session;
    IHS_SessionPacket packet;
    IHS_SessionPacketInitialize(session, &packet);
    packet.header.hasCrc = hasCrc;
    packet.header.type = type;
    packet.header.channelId = channel->id;
    if (packetId == IHS_PACKET_ID_NEXT) {
        packet.header.packetId = IHS_SessionChannelNextPacketId(channel);
    } else {
        packet.header.packetId = packetId;
    }
    packet.body = body;
    packet.bodyLen = bodyLen;
    if (padTo) {
        IHS_SessionPacketPadTo(&packet, padTo);
    }
    IHS_SessionSendPacket(session, &packet);
}

void IHS_SessionChannelPacketAck(IHS_SessionChannel *channel, int32_t packetId, bool ok) {
    uint8_t body[4];
    IHS_WriteUInt32LE(body, IHS_SessionPacketTimestamp(channel->session));
    IHS_SessionChannelSendBytes(channel, ok ? IHS_SessionPacketTypeACK : IHS_SessionPacketTypeNACK, true, packetId,
                                body, 4, 0);
}