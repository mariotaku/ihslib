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

#include <memory.h>
#include <assert.h>
#include <stdlib.h>

#include "channel.h"
#include "endianness.h"
#include "ihs_buffer.h"
#include "ihs_buffer_ext.h"

#include "session/session_pri.h"
#include "session/frame.h"

static bool SessionChannelSendFrameFragmented(IHS_SessionChannel *channel, IHS_SessionFrame *frame, size_t bodyLimit,
                                              bool enableRetransmit);

static IHS_SessionPacketType FragmentedPacketType(IHS_SessionPacketType type);

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

void IHS_SessionChannelStop(IHS_SessionChannel *channel) {
    if (channel->cls->stopped == NULL) {
        return;
    }
    channel->cls->stopped(channel);
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
    IHS_SessionLog(session, IHS_LogLevelInfo, "Channel", "Adding channel %u", channel->id);
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
    IHS_SessionLog(session, IHS_LogLevelInfo, "Channel", "Removing channel %u", channelId);
    IHS_SessionChannel *channelToRemove = session->channels[channelIndex];
    IHS_SessionChannelStop(channelToRemove);
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

void IHS_SessionChannelInitializePacketHeader(IHS_SessionChannel *channel, IHS_SessionPacketHeader *header,
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
}

bool IHS_SessionChannelInitializePacket(IHS_SessionChannel *channel, IHS_SessionPacket *packet,
                                        IHS_SessionPacketType type, bool hasCrc, int32_t packetId) {
    IHS_SessionChannelInitializePacketHeader(channel, &packet->header, type, hasCrc, packetId);
    IHS_SessionPacketBodyInitialize(&packet->body, hasCrc);
    return true;
}

bool IHS_SessionChannelInitializeFrame(IHS_SessionChannel *channel, IHS_SessionFrame *frame,
                                       IHS_SessionPacketType type, bool hasCrc, int32_t packetId) {
    IHS_SessionChannelInitializePacketHeader(channel, &frame->header, type, hasCrc, packetId);
    IHS_SessionFrameBodyInitialize(&frame->body, hasCrc);
    return true;
}

bool IHS_SessionChannelSendPacket(IHS_SessionChannel *channel, IHS_SessionPacket *packet, bool enableRetransmit) {
    return IHS_SessionQueuePacket(channel->session, packet, enableRetransmit);
}

bool IHS_SessionChannelSendFrame(IHS_SessionChannel *channel, IHS_SessionFrame *frame, bool enableRetransmit) {
    size_t mtu = channel->session->state.mtu > 0 ? channel->session->state.mtu : 1024;
    int packetBodySizeLimit = (int) mtu - IHS_PACKET_HEADER_SIZE;
    if (frame->header.hasCrc) {
        packetBodySizeLimit -= 4;
    }
    assert(packetBodySizeLimit > 0);

    bool ret;
    if (frame->body.size < packetBodySizeLimit) {
        // Frame can fit in single packet
        IHS_SessionPacket packet;
        packet.header = frame->header;
        IHS_BufferTransferOwnership(&frame->body, &packet.body);
        ret = IHS_SessionQueuePacket(channel->session, &packet, enableRetransmit);
        IHS_SessionPacketClear(&packet, true);
    } else {
        ret = SessionChannelSendFrameFragmented(channel, frame, packetBodySizeLimit, enableRetransmit);
    }
    return ret;
}

void IHS_SessionChannelPacketAck(IHS_SessionChannel *channel, int32_t packetId, bool ok) {
    IHS_SessionPacket packet;
    IHS_SessionPacketType type = ok ? IHS_SessionPacketTypeACK : IHS_SessionPacketTypeNACK;
    IHS_SessionChannelInitializePacket(channel, &packet, type, true, packetId);
    IHS_BufferAppendUInt32LE(&packet.body, IHS_SessionPacketTimestamp());
    IHS_SessionChannelSendPacket(channel, &packet, false);
    IHS_SessionPacketClear(&packet, true);
}

static bool SessionChannelSendFrameFragmented(IHS_SessionChannel *channel, IHS_SessionFrame *frame, size_t bodyLimit,
                                              bool enableRetransmit) {
    int fragmentSize = (int) (frame->body.size / bodyLimit + 1);
    assert(fragmentSize <= INT16_MAX);
    int16_t fragmentId = -1;
    while (frame->body.size != 0) {
        size_t packetBodySize = frame->body.size > bodyLimit ? bodyLimit : frame->body.size;
        IHS_SessionPacket packet;
        packet.header = frame->header;
        if (fragmentId < 0) {
            packet.header.fragmentId = (int16_t) fragmentSize;
        } else {
            packet.header.fragmentId = fragmentId;
            packet.header.type = FragmentedPacketType(packet.header.type);
        }
        IHS_SessionPacketBodyInitialize(&packet.body, packet.header.hasCrc);
        IHS_BufferAppendMem(&packet.body, IHS_BufferPointer(&frame->body), packetBodySize);
        bool ret = IHS_SessionQueuePacket(channel->session, &packet, enableRetransmit);
        IHS_SessionPacketClear(&packet, true);
        if (!ret) {
            return false;
        }
        IHS_BufferOffsetBy(&frame->body, (int) packetBodySize);
        fragmentId += 1;
    }
    return true;
}

static IHS_SessionPacketType FragmentedPacketType(IHS_SessionPacketType type) {
    switch (type) {
        case IHS_SessionPacketTypeUnreliable:
            return IHS_SessionPacketTypeUnreliableFrag;
        case IHS_SessionPacketTypeReliable:
            return IHS_SessionPacketTypeReliableFrag;
        default: {
            abort();
        }
    }
}