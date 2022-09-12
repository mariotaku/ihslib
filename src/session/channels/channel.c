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
#include <stdlib.h>

#include "channel.h"
#include "session/session_pri.h"
#include "endianness.h"
#include "ihs_buffer.h"
#include "session/frame.h"


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

void IHS_SessionChannelReceivedPacket(IHS_SessionChannel *channel, IHS_SessionPacket *packet) {
    channel->cls->received(channel, packet);
}

void IHS_SessionChannelReceivedPacketNoop(IHS_SessionChannel *channel, IHS_SessionPacket *packet) {
    IHS_UNUSED(channel);
    IHS_UNUSED(packet);
}

uint16_t IHS_SessionChannelNextPacketId(IHS_SessionChannel *channel) {
    return channel->nextPacketId++;
}

bool IHS_SessionChannelPacketHeaderInitialize(IHS_SessionChannel *channel, IHS_SessionPacketHeader *header,
                                              IHS_SessionPacketType type, bool hasCrc, int32_t packetId) {
    memset(header, 0, sizeof(IHS_SessionPacketHeader));
    const IHS_Session *session = channel->session;
    if (type != IHS_SessionPacketTypeUnconnected) {
        header->srcConnectionId = session->state.connectionId;
        header->dstConnectionId = session->state.hostConnectionId;
    }
    header->sendTimestamp = IHS_SessionPacketTimestamp();
    header->hasCrc = hasCrc;
    header->type = type;

    header->channelId = channel->id;
    if (packetId == IHS_PACKET_ID_NEXT) {
        header->packetId = IHS_SessionChannelNextPacketId(channel);
    } else {
        header->packetId = packetId;
    }
    return true;
}

bool IHS_SessionChannelPacketInitialize(IHS_SessionChannel *channel, IHS_SessionPacket *packet,
                                        IHS_SessionPacketType type, bool hasCrc, int32_t packetId) {
    IHS_SessionChannelPacketHeaderInitialize(channel, &packet->header, type, hasCrc, packetId);
    IHS_BufferInit(&packet->body, 2048, 2048);

    // Reserve space for serialized header
    IHS_BufferFillMem(&packet->body, 0, 0, IHS_PACKET_HEADER_SIZE);
    IHS_BufferOffsetBy(&packet->body, IHS_PACKET_HEADER_SIZE);

    assert(packet->body.offset == IHS_PACKET_HEADER_SIZE);
    return true;
}

bool IHS_SessionChannelFrameInitialize(IHS_SessionChannel *channel, IHS_SessionFrame *frame,
                                       IHS_SessionPacketType type, bool hasCrc, int32_t packetId) {
    IHS_SessionChannelPacketHeaderInitialize(channel, &frame->header, type, hasCrc, packetId);
    IHS_BufferInit(&frame->body, 2048, 1024 * 1024);

    // Reserve space for serialized header
    IHS_BufferFillMem(&frame->body, 0, 0, IHS_PACKET_HEADER_SIZE);
    IHS_BufferOffsetBy(&frame->body, IHS_PACKET_HEADER_SIZE);

    assert(frame->body.offset == IHS_PACKET_HEADER_SIZE);
    return true;
}

bool IHS_SessionChannelSendPacket(IHS_SessionChannel *channel, IHS_SessionPacket *packet) {
    return IHS_SessionSendPacket(channel->session, packet);
}

bool IHS_SessionChannelSendFrame(IHS_SessionChannel *channel, IHS_SessionFrame *frame) {
    size_t singlePacketSize = IHS_PACKET_HEADER_SIZE + frame->body.size;
    if (frame->header.hasCrc) {
        singlePacketSize += 4;
    }
    if (channel->session->state.mtu > 0 && singlePacketSize > channel->session->state.mtu) {
        IHS_SessionLog(channel->session, IHS_LogLevelFatal, "Session", "Can't send packet larger than MTU");
        abort();
    } else if (singlePacketSize > 1536) {
        IHS_SessionLog(channel->session, IHS_LogLevelFatal, "Session", "Can't send large packet before MTU known");
        abort();
    }
    IHS_SessionPacket packet;
    packet.header = frame->header;
    IHS_BufferTransferOwnership(&frame->body, &packet.body);
    bool ret = IHS_SessionSendPacket(channel->session, &packet);
    IHS_SessionPacketClear(&packet, true);
    return ret;
}

bool IHS_SessionChannelSendBytes(IHS_SessionChannel *channel, IHS_SessionPacketType type, bool hasCrc, int32_t packetId,
                                 const uint8_t *body, size_t bodyLen, size_t padTo) {
    IHS_Session *session = channel->session;
    IHS_SessionPacket packet;
    IHS_SessionChannelPacketInitialize(channel, &packet, type, hasCrc, packetId);
    IHS_BufferAppendMem(&packet.body, body, bodyLen);
    if (padTo) {
        IHS_SessionPacketPadTo(&packet, padTo);
    }
    bool ret = IHS_SessionSendPacket(session, &packet);
    IHS_SessionPacketClear(&packet, true);
    return ret;
}

void IHS_SessionChannelPacketAck(IHS_SessionChannel *channel, int32_t packetId, bool ok) {
    IHS_SessionPacket packet;
    IHS_SessionPacketType type = ok ? IHS_SessionPacketTypeACK : IHS_SessionPacketTypeNACK;
    IHS_SessionChannelPacketInitialize(channel, &packet, type, true, packetId);
    IHS_BufferAppendUInt32LE(&packet.body, IHS_SessionPacketTimestamp());
    IHS_SessionChannelSendPacket(channel, &packet);
    IHS_SessionPacketClear(&packet, true);
}