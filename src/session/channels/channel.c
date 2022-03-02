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


IHS_SessionChannel *IHS_SessionChannelCreate(const IHS_SessionChannelClass *cls, IHS_Session *session,
                                             IHS_SessionChannelId id) {
    assert(cls->instanceSize >= sizeof(IHS_SessionChannel));
    IHS_SessionChannel *channel = malloc(cls->instanceSize);
    memset(channel, 0, sizeof(IHS_SessionChannel));
    channel->cls = *cls;
    channel->session = session;
    channel->id = id;
    return channel;
}

void IHS_SessionChannelDestroy(IHS_SessionChannel *channel) {
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

void IHS_SessionChannelSendBytes(IHS_SessionChannel *channel, IHS_SessionPacketType type, bool hasCrc,
                                 const uint8_t *body, size_t bodyLen) {
    IHS_Session *session = channel->session;
    IHS_SessionPacket packet;
    IHS_SessionPacketInitialize(session, &packet);
    packet.header.hasCrc = hasCrc;
    packet.header.type = type;
    packet.header.channelId = channel->id;
    packet.body = body;
    packet.bodyLen = bodyLen;

    IHS_SessionSendPacket(session, &packet);
}