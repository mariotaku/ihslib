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

#include <stdlib.h>
#include <memory.h>

#include "ch_data.h"
#include "ch_data_audio.h"
#include "ch_data_video.h"
#include "client/client_pri.h"
#include "endianness.h"

static void DataThreadWorker(IHS_SessionChannelData *channel);

static void DataThreadInterrupt(IHS_SessionChannelData *channel);

static void ReceivedFrame(IHS_SessionChannelData *channel, const IHS_SessionFrame *frame);

IHS_SessionChannel *IHS_SessionChannelDataCreate(const IHS_SessionChannelDataClass *cls, IHS_Session *session,
                                                 IHS_SessionChannelType type, IHS_SessionChannelId id,
                                                 const void *config) {
    assert(cls->base.instanceSize >= sizeof(IHS_SessionChannelData));
    return IHS_SessionChannelCreate(&cls->base, session, type, id, config);
}

void IHS_SessionChannelDataInit(IHS_SessionChannel *channel) {
    IHS_SessionChannelData *dataCh = (IHS_SessionChannelData *) channel;
    uv_mutex_init(&dataCh->mutex);
    uv_cond_init(&dataCh->cond);
    dataCh->window = IHS_SessionPacketsWindowCreate(1024);
    dataCh->threadInterrupted = false;
    uv_thread_create(&dataCh->workerThread, (void (*)(void *)) DataThreadWorker, dataCh);
}

void IHS_SessionChannelDataDeinit(IHS_SessionChannel *channel) {
    IHS_SessionChannelData *dataCh = (IHS_SessionChannelData *) channel;

    DataThreadInterrupt(dataCh);

    uv_thread_join(&dataCh->workerThread);
    IHS_SessionPacketsWindowDestroy(dataCh->window);
    uv_cond_destroy(&dataCh->cond);
    uv_mutex_destroy(&dataCh->mutex);
}

void IHS_SessionChannelDataReceived(struct IHS_SessionChannel *channel, const IHS_SessionPacket *packet) {
    IHS_SessionChannelData *dataCh = (IHS_SessionChannelData *) channel;
    uv_mutex_lock(&dataCh->mutex);
    if (!IHS_SessionPacketsWindowAdd(dataCh->window, packet)) {
        uv_mutex_unlock(&dataCh->mutex);
        return;
    }
    uv_cond_signal(&dataCh->cond);
    uv_mutex_unlock(&dataCh->mutex);
}


size_t IHS_SessionChannelDataFrameHeaderParse(IHS_SessionDataFrameHeader *header, const uint8_t *data) {
    size_t offset = 0;
    offset += IHS_ReadUInt16LE(&data[offset], &header->id);
    offset += IHS_ReadUInt32LE(&data[offset], &header->timestamp);
    offset += IHS_ReadUInt16LE(&data[offset], &header->inputMark);
    offset += IHS_ReadUInt32LE(&data[offset], &header->inputRecvTimestamp);
    return offset;
}

void IHS_SessionChannelControlOnDataControl(IHS_SessionChannel *channel, EStreamControlMessage type,
                                            const uint8_t *payload, size_t payloadLen,
                                            const IHS_SessionPacketHeader *header) {
    IHS_UNUSED(header);
    IHS_Session *session = channel->session;
    switch (type) {
        case k_EStreamControlStartAudioData: {
            IHS_SessionChannel *audio = IHS_SessionChannelForType(session, IHS_SessionChannelTypeDataAudio);
            if (audio) break;
            CStartAudioDataMsg *message = cstart_audio_data_msg__unpack(NULL, payloadLen, payload);
            audio = IHS_SessionChannelDataAudioCreate(session, message);
            IHS_SessionChannelAdd(session, audio);
            cstart_audio_data_msg__free_unpacked(message, NULL);
            break;
        }
        case k_EStreamControlStopAudioData: {
            IHS_SessionChannel *audio = IHS_SessionChannelForType(session, IHS_SessionChannelTypeDataAudio);
            if (!audio) break;
            IHS_SessionChannelRemove(session, audio->id);
            break;
        }
        case k_EStreamControlStartVideoData: {
            IHS_SessionChannel *video = IHS_SessionChannelForType(session, IHS_SessionChannelTypeDataVideo);
            if (video) break;
            CStartVideoDataMsg *message = cstart_video_data_msg__unpack(NULL, payloadLen, payload);
            video = IHS_SessionChannelDataVideoCreate(session, message);
            IHS_SessionChannelAdd(session, video);
            cstart_video_data_msg__free_unpacked(message, NULL);
            break;
        }
        case k_EStreamControlStopVideoData: {
            IHS_SessionChannel *video = IHS_SessionChannelForType(session, IHS_SessionChannelTypeDataVideo);
            if (!video) break;
            IHS_SessionChannelRemove(session, video->id);
            break;
        }
        default: {
            abort();
        }
    }
}


static void DataThreadWorker(IHS_SessionChannelData *channel) {
    const IHS_SessionChannelDataClass *cls = (const IHS_SessionChannelDataClass *) channel->base.cls;
    IHS_SessionFrame frame;
    cls->start((IHS_SessionChannel *) channel);
    uv_mutex_lock(&channel->mutex);
    while (!channel->threadInterrupted) {
        uv_cond_wait(&channel->cond, &channel->mutex);
        for (IHS_SessionPacketsWindowDiscard(channel->window, IHS_SESSION_PACKET_TIMESTAMP_FROM_MILLIS(50));
             IHS_SessionPacketsWindowPoll(channel->window, &frame);
             IHS_SessionPacketsWindowReleaseFrame(&frame)) {
            ReceivedFrame(channel, &frame);
        }
    }
    uv_mutex_unlock(&channel->mutex);
    cls->stop((IHS_SessionChannel *) channel);
}

static void DataThreadInterrupt(IHS_SessionChannelData *channel) {
    uv_mutex_lock(&channel->mutex);
    channel->threadInterrupted = true;
    uv_cond_signal(&channel->cond);
    uv_mutex_unlock(&channel->mutex);
}

static void ReceivedFrame(IHS_SessionChannelData *channel, const IHS_SessionFrame *frame) {
    assert (frame->header.type == IHS_SessionPacketTypeUnreliable);
    size_t bodyOffset = 0;
    EStreamDataMessage type = frame->body[bodyOffset++];
    if (type != k_EStreamDataPacket) {
        return;
    }
    IHS_SessionDataFrameHeader header;
    bool hasHeader = false;
    if (frame->bodyLen - bodyOffset > IHS_SESSION_DATA_FRAME_HEADER_SIZE) {
        hasHeader = true;
        bodyOffset += IHS_SessionChannelDataFrameHeaderParse(&header, &frame->body[bodyOffset]);
    }
    const IHS_SessionChannelDataClass *cls = (const IHS_SessionChannelDataClass *) channel->base.cls;
    cls->dataFrame((IHS_SessionChannel *) channel, hasHeader ? &header : NULL, &frame->body[bodyOffset],
                   frame->bodyLen - bodyOffset);
}
