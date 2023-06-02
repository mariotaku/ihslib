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

#include <stdio.h>

#include "ch_data.h"
#include "endianness.h"
#include "session/session_pri.h"

#include "ihs_buffer_ext.h"

#define DISCARD_DIFF IHS_SESSION_PACKET_TIMESTAMP_FROM_MILLIS(200)

static void DataThreadWorker(IHS_SessionChannelData *channel);

static void DataThreadInterrupt(IHS_SessionChannelData *channel);

static void ReceivedFrame(IHS_SessionChannelData *channel, IHS_SessionFrame *frame);

static const char *DataChannelName(IHS_SessionChannelType type);

IHS_SessionChannel *IHS_SessionChannelDataCreate(const IHS_SessionChannelDataClass *cls, IHS_Session *session,
                                                 IHS_SessionChannelType type, IHS_SessionChannelId id,
                                                 const void *config) {
    assert(cls->base.instanceSize >= sizeof(IHS_SessionChannelData));
    return IHS_SessionChannelCreate(&cls->base, session, type, id, config);
}

void IHS_SessionChannelDataInit(IHS_SessionChannel *channel, uint16_t windowCapacity) {
    IHS_SessionChannelData *dataCh = (IHS_SessionChannelData *) channel;
    dataCh->lock = IHS_MutexCreate();
    dataCh->windowLock = IHS_MutexCreate();
    dataCh->windowCond = IHS_CondCreate();
    dataCh->window = IHS_SessionPacketsWindowCreate(windowCapacity);
    dataCh->interrupted = false;
    char threadName[16];
    snprintf(threadName, 16, "IHS%s", DataChannelName(channel->type));
    dataCh->worker = IHS_ThreadCreate((IHS_ThreadFunction *) DataThreadWorker, threadName, dataCh);
}

void IHS_SessionChannelDataDeinit(IHS_SessionChannel *channel) {
    IHS_SessionChannelData *dataCh = (IHS_SessionChannelData *) channel;

    assert(dataCh->interrupted);

    IHS_ThreadJoin(dataCh->worker);
    IHS_SessionPacketsWindowDestroy(dataCh->window);
    IHS_MutexDestroy(dataCh->windowLock);
    IHS_CondDestroy(dataCh->windowCond);
    dataCh->window = NULL;
    IHS_MutexDestroy(dataCh->lock);
}

void IHS_SessionChannelDataReceived(IHS_SessionChannel *channel, IHS_SessionPacket *packet) {
    IHS_SessionChannelData *dataCh = (IHS_SessionChannelData *) channel;
    assert(dataCh->window != NULL);
    IHS_SessionPacketType type = packet->header.type;
    assert(type == IHS_SessionPacketTypeUnreliable || type == IHS_SessionPacketTypeUnreliableFrag);
    IHS_MutexLock(dataCh->windowLock);
    if (!IHS_SessionPacketsWindowAdd(dataCh->window, packet)) {
        IHS_SessionLog(channel->session, IHS_LogLevelWarn, "Data", "%s channel items overflow! Available: %u",
                       DataChannelName(channel->type), IHS_SessionPacketsWindowAvailable(dataCh->window));
        IHS_SessionPacketsWindowDiscard(dataCh->window, 0);
        IHS_SessionChannelDataLost(channel);
    }
    IHS_CondSignal(dataCh->windowCond);
    IHS_MutexUnlock(dataCh->windowLock);
    dataCh->lastPacketTimestamp = packet->header.sendTimestamp;
}

void IHS_SessionChannelDataLost(IHS_SessionChannel *channel) {
    CStreamDataLostMsg message = CSTREAM_DATA_LOST_MSG__INIT;
    IHS_SessionPacket packet;
    IHS_SessionChannelInitializePacket(channel, &packet, IHS_SessionPacketTypeUnreliable, true, IHS_PACKET_ID_NEXT);
    IHS_BufferAppendUInt8(&packet.body, k_EStreamDataLost);
    IHS_BufferAppendMessage(&packet.body, (const ProtobufCMessage *) &message);
    IHS_SessionChannelQueuePacket(channel, &packet, false);
    IHS_SessionPacketClear(&packet, true);
}

void IHS_SessionChannelDataStopped(IHS_SessionChannel *channel) {
    DataThreadInterrupt((IHS_SessionChannelData *) channel);
}

size_t IHS_SessionChannelDataFrameHeaderParse(IHS_SessionDataFrameHeader *header, const IHS_Buffer *data) {
    size_t offset = 0;
    offset += IHS_ReadUInt16LE(IHS_BufferPointerAt(data, offset), &header->id);
    offset += IHS_ReadUInt32LE(IHS_BufferPointerAt(data, offset), &header->timestamp);
    offset += IHS_ReadUInt16LE(IHS_BufferPointerAt(data, offset), &header->inputMark);
    offset += IHS_ReadUInt32LE(IHS_BufferPointerAt(data, offset), &header->inputRecvTimestamp);
    return offset;
}

static void DataThreadWorker(IHS_SessionChannelData *channel) {
    const IHS_SessionChannelDataClass *cls = (const IHS_SessionChannelDataClass *) channel->base.cls;
    IHS_SessionFrame frame;
    IHS_BufferInit(&frame.body, 1024, 1024 * 1024);
    const char *channelName = DataChannelName(channel->base.type);
    IHS_SessionLog(channel->base.session, IHS_LogLevelInfo, "Data", "Starting %s channel", channelName);
    if (!cls->start((IHS_SessionChannel *) channel)) {
        IHS_SessionLog(channel->base.session, IHS_LogLevelError, "Data", "Failed to start %s channel", channelName);
        IHS_SessionDisconnect(channel->base.session);
        return;
    }
    IHS_SessionLog(channel->base.session, IHS_LogLevelInfo, "Data", "%s channel started", channelName);
    while (!channel->interrupted) {
        IHS_MutexLock(channel->windowLock);
        uint16_t discarded = IHS_SessionPacketsWindowDiscard(channel->window, DISCARD_DIFF);
        if (discarded > 0) {
            IHS_SessionLog(channel->base.session, IHS_LogLevelDebug, channelName, "Discarded %u packets", discarded);
        }
        bool hasFrame;
        while (!(hasFrame = IHS_SessionPacketsWindowPoll(channel->window, &frame))) {
            IHS_CondWait(channel->windowCond, channel->windowLock);
            if (channel->interrupted) {
                break;
            }
        }
        IHS_MutexUnlock(channel->windowLock);
        if (hasFrame) {
            ReceivedFrame(channel, &frame);
            IHS_SessionPacketsWindowReleaseFrame(&frame);
        }
    }
    IHS_SessionLog(channel->base.session, IHS_LogLevelInfo, "Data", "Stopping %s channel", channelName);
    IHS_BufferClear(&frame.body, true);
    cls->stop((IHS_SessionChannel *) channel);
}

static void DataThreadInterrupt(IHS_SessionChannelData *channel) {
    if (channel->interrupted) {
        return;
    }
    IHS_MutexLock(channel->lock);
    channel->interrupted = true;
    IHS_MutexUnlock(channel->lock);

    IHS_MutexLock(channel->windowLock);
    IHS_CondSignal(channel->windowCond);
    IHS_MutexUnlock(channel->windowLock);
}

static void ReceivedFrame(IHS_SessionChannelData *channel, IHS_SessionFrame *frame) {
    assert(frame->header.type == IHS_SessionPacketTypeUnreliable);
    EStreamDataMessage type = *IHS_BufferPointer(&frame->body);
    IHS_BufferOffsetBy(&frame->body, 1);
    if (type != k_EStreamDataPacket) {
        return;
    }
    IHS_SessionDataFrameHeader header;
    bool hasHeader = false;
    if (frame->body.size > IHS_SESSION_DATA_FRAME_HEADER_SIZE) {
        hasHeader = true;
        size_t offset = IHS_SessionChannelDataFrameHeaderParse(&header, &frame->body);
        IHS_BufferOffsetBy(&frame->body, (int) offset);
    }
    const IHS_SessionChannelDataClass *cls = (const IHS_SessionChannelDataClass *) channel->base.cls;
    cls->dataFrame((IHS_SessionChannel *) channel, hasHeader ? &header : NULL, &frame->body);
}

static const char *DataChannelName(IHS_SessionChannelType type) {
    switch (type) {
        case IHS_SessionChannelTypeDataAudio:
            return "Audio";
        case IHS_SessionChannelTypeDataVideo:
            return "Video";
        default:
            return "Data";
    }
}