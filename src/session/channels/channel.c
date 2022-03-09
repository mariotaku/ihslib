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
                                             IHS_SessionChannelType type, IHS_SessionChannelId id, const void *config) {
    assert(cls->instanceSize >= sizeof(IHS_SessionChannel));
    IHS_SessionChannel *channel = malloc(cls->instanceSize);
    memset(channel, 0, cls->instanceSize);
    channel->cls = cls;
    channel->type = type;
    channel->id = id;
    channel->session = session;
    if (cls->init) {
        cls->init(channel, config);
    }
    return channel;
}

void IHS_SessionChannelDestroy(IHS_SessionChannel *channel) {
    if (channel->cls->deinit) {
        channel->cls->deinit(channel);
    }
    free(channel);
}

IHS_SessionChannel *IHS_SessionChannelFor(IHS_Session *session, IHS_SessionChannelId channelId) {
    if (channelId <= IHS_SessionChannelIdStats) {
        return session->channels[channelId];
    }
    for (int i = 0; i < session->numChannels; ++i) {
        IHS_SessionChannel *channel = session->channels[i];
        if (channel->id == channelId) return channel;
    }
    return NULL;
}

IHS_SessionChannel *IHS_SessionChannelForType(IHS_Session *session, IHS_SessionChannelType channelType) {
    if (channelType <= IHS_SessionChannelTypeStats) {
        return session->channels[channelType];
    }
    for (int i = 0; i < session->numChannels; ++i) {
        IHS_SessionChannel *channel = session->channels[i];
        if (channel->type == channelType) return channel;
    }
    return NULL;
}

void IHS_SessionChannelAdd(IHS_Session *session, IHS_SessionChannel *channel) {
    if (IHS_SessionChannelFor(session, channel->id)) {
        return;
    }
    session->channels[session->numChannels] = channel;
    session->numChannels++;
}

void IHS_SessionChannelRemove(IHS_Session *session, IHS_SessionChannelId channelId) {
    if (channelId < IHS_SessionChannelIdDataStart) return;
    int channelIndex = -1;
    for (int i = 0; i < session->numChannels; ++i) {
        IHS_SessionChannel *channel = session->channels[i];
        if (channel->id == channelId) {
            channelIndex = i;
            break;
        }
    }
    if (channelIndex < 0) return;
    IHS_SessionChannel *channelToRemove = session->channels[channelIndex];
    IHS_SessionChannelDestroy(channelToRemove);
    int remaining = session->numChannels - 1 - channelIndex;
    if (remaining > 0) {
        memmove(&session->channels[channelIndex], &session->channels[channelIndex + 1],
                remaining * sizeof(IHS_SessionChannel *));
    } else {
        session->numChannels--;
    }
}

void IHS_SessionChannelReceivedPacket(IHS_SessionChannel *channel, const IHS_SessionPacket *packet) {
    channel->cls->received(channel, packet);
}

void IHS_SessionChannelReceivedPacketNoop(IHS_SessionChannel *channel, const IHS_SessionPacket *packet) {
}

uint16_t IHS_SessionChannelNextPacketId(IHS_SessionChannel *channel) {
    return channel->nextPacketId++;
}

bool IHS_SessionChannelSendBytes(IHS_SessionChannel *channel, IHS_SessionPacketType type, bool hasCrc, int32_t packetId,
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
    return IHS_SessionSendPacket(session, &packet);
}

void IHS_SessionChannelPacketAck(IHS_SessionChannel *channel, int32_t packetId, bool ok) {
    uint8_t body[4];
    IHS_WriteUInt32LE(body, IHS_SessionPacketTimestamp(channel->session));
    IHS_SessionChannelSendBytes(channel, ok ? IHS_SessionPacketTypeACK : IHS_SessionPacketTypeNACK, true, packetId,
                                body, 4, 0);
}