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

#include "session/channels/ch_data.h"
#include "ch_data_video.h"

#include "crypto.h"
#include "endianness.h"
#include "session/channels/video/callback_h264.h"
#include "session/channels/ch_stats.h"
#include "protobuf/pb_utils.h"


static void ChannelVideoInit(IHS_SessionChannel *channel, const void *config);

static void ChannelVideoDeinit(IHS_SessionChannel *channel);

static bool DataStart(IHS_SessionChannel *channel);

static void DataReceived(IHS_SessionChannel *channel, const IHS_SessionDataFrameHeader *header,
                         const uint8_t *data, size_t len);

static void DataStop(IHS_SessionChannel *channel);

static size_t VideoFrameHeaderParse(IHS_SessionVideoFrameHeader *header, const uint8_t *data);

static void PreprocessAndSubmit(IHS_SessionChannel *channel, const uint8_t *data, size_t len,
                                const IHS_SessionVideoFrameHeader *header);

static uint64_t ReportVideoStats(void *data);

static const IHS_SessionChannelDataClass ChannelClass = {
        {
                .init = ChannelVideoInit,
                .deinit = ChannelVideoDeinit,
                .received = IHS_SessionChannelDataReceived,
                .instanceSize = sizeof(IHS_SessionChannelVideo)
        },
        .start = DataStart,
        .dataFrame = DataReceived,
        .stop = DataStop,
};

static const uint8_t EmptyIV[16] = {
        0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0,
};

IHS_SessionChannel *IHS_SessionChannelDataVideoCreate(IHS_Session *session, const CStartVideoDataMsg *message) {
    return IHS_SessionChannelDataCreate(&ChannelClass, session, IHS_SessionChannelTypeDataVideo,
                                        message->channel, (void *) message);
}

static void ChannelVideoInit(IHS_SessionChannel *channel, const void *config) {
    IHS_SessionChannelVideo *videoCh = (IHS_SessionChannelVideo *) channel;
    const CStartVideoDataMsg *message = config;
    videoCh->config.width = message->width;
    videoCh->config.height = message->height;
    videoCh->config.codec = (IHS_StreamVideoCodec) message->codec;
    if (message->has_codec_data) {
        videoCh->config.codecDataLen = message->codec_data.len;
        videoCh->config.codecData = malloc(message->codec_data.len);
        memcpy(videoCh->config.codecData, message->codec_data.data, message->codec_data.len);
    }
    videoCh->stateMutex = IHS_MutexCreate();
    IHS_SessionChannelDataInit(channel);
}

static void ChannelVideoDeinit(IHS_SessionChannel *channel) {
    IHS_SessionChannelDataDeinit(channel);
    IHS_SessionChannelVideo *videoCh = (IHS_SessionChannelVideo *) channel;
    IHS_MutexDestroy(videoCh->stateMutex);
    if (videoCh->config.codecData) {
        free(videoCh->config.codecData);
    }
}

static bool DataStart(IHS_SessionChannel *channel) {
    IHS_SessionChannelVideo *videoCh = (IHS_SessionChannelVideo *) channel;
    IHS_Session *session = channel->session;
    const IHS_StreamVideoCallbacks *callbacks = session->callbacks.video;
    if (!callbacks || !callbacks->start) return true;
    if (callbacks->start(session, &videoCh->config, session->callbackContexts.video) != 0) {
        return false;
    }
    CVideoDecoderInfoMsg message = CVIDEO_DECODER_INFO_MSG__INIT;
    message.info = "Marvell hardware decoding";
    PROTOBUF_C_SET_VALUE(message, threads, 1);

    videoCh->statsTimer = IHS_TimerStart(session->base.timers, ReportVideoStats, NULL, 1000, videoCh);

    return IHS_SessionSendControlMessage(session, k_EStreamControlVideoDecoderInfo,
                                         (const ProtobufCMessage *) &message, IHS_PACKET_ID_NEXT);
}

static void DataReceived(IHS_SessionChannel *channel, const IHS_SessionDataFrameHeader *header, const uint8_t *data,
                         size_t len) {
    IHS_SessionChannelVideo *videoCh = (IHS_SessionChannelVideo *) channel;
    size_t offset = 0;
    IHS_SessionVideoFrameHeader vhead;
    offset += VideoFrameHeaderParse(&vhead, &data[offset]);

    IHS_MutexLock(videoCh->stateMutex);

    if (vhead.flags & VideoFrameFlagKeyFrame) {
        videoCh->waitingKeyFrame = false;
        videoCh->expectedSequence = vhead.sequence;
    }
    if (videoCh->waitingKeyFrame) {
        goto unlock;
    }
    int seqDiff = ((int) vhead.sequence) - videoCh->expectedSequence;
    if (seqDiff != 0) {
        if (seqDiff > INT16_MAX || seqDiff < INT16_MIN) {
            IHS_SessionLog(channel->session, IHS_BaseLogLevelWarn, "Unexpected video frame sequence %u (expect %u), "
                                                                   "ignoring because the difference is too large",
                           vhead.sequence, videoCh->expectedSequence);
            goto unlock;
        }
        IHS_SessionLog(channel->session, IHS_BaseLogLevelWarn, "Unexpected video frame sequence %u (expect %u), "
                                                               "request keyframe", vhead.sequence,
                       videoCh->expectedSequence);
        IHS_SessionChannelDataLost(channel);
        videoCh->waitingKeyFrame = true;
        goto unlock;
    }
    videoCh->lastFrameId = header->id;
    videoCh->expectedSequence++;
    videoCh->frameCounter++;
    if (vhead.flags & VideoFrameFlagEncrypted) {
        const IHS_SessionConfig *config = &channel->session->config;
        uint8_t *decrypted = malloc(len);
        size_t decryptedLen = len;
        IHS_CryptoSymmetricDecryptWithIV(data, len, EmptyIV, sizeof(EmptyIV),
                                         config->sessionKey, config->sessionKeyLen, decrypted, &decryptedLen);
        PreprocessAndSubmit(channel, decrypted, decryptedLen, &vhead);
        free(decrypted);
    } else {
        PreprocessAndSubmit(channel, &data[offset], len - offset, &vhead);
    }
    unlock:
    IHS_MutexUnlock(videoCh->stateMutex);
}

static void DataStop(IHS_SessionChannel *channel) {
    IHS_Session *session = channel->session;
    IHS_SessionChannelVideo *videoCh = (IHS_SessionChannelVideo *) channel;
    if (videoCh->statsTimer != NULL) {
        IHS_TimerStop(videoCh->statsTimer);
        videoCh->statsTimer = NULL;
    }
    const IHS_StreamVideoCallbacks *callbacks = session->callbacks.video;
    if (!callbacks || !callbacks->stop) return;
    callbacks->stop(session, session->callbackContexts.video);
}

static size_t VideoFrameHeaderParse(IHS_SessionVideoFrameHeader *header, const uint8_t *data) {
    size_t offset = 0;
    offset += IHS_ReadUInt16LE(&data[offset], &header->sequence);
    header->flags = data[offset++];
    offset += IHS_ReadUInt16LE(&data[offset], &header->reserved1);
    offset += IHS_ReadUInt16LE(&data[offset], &header->reserved2);
    return offset;
}


static void PreprocessAndSubmit(IHS_SessionChannel *channel, const uint8_t *data, size_t len,
                                const IHS_SessionVideoFrameHeader *header) {
    IHS_SessionChannelVideo *videoCh = (IHS_SessionChannelVideo *) channel;
    assert (videoCh->config.codec == IHS_StreamVideoCodecH264);
    IHS_SessionVideoFrameSubmitH264(channel, data, len, header);
}

static uint64_t ReportVideoStats(void *data) {
    IHS_SessionChannel *channel = data;
    IHS_SessionChannelVideo *videoCh = (IHS_SessionChannelVideo *) channel;
    IHS_MutexLock(videoCh->stateMutex);

    IHS_SessionChannel *statsCh = IHS_SessionChannelFor(channel->session, IHS_SessionChannelIdStats);
    CFrameStatsListMsg message = CFRAME_STATS_LIST_MSG__INIT;
    message.data_type = k_EStreamingVideoData;
    message.latest_frame_id = videoCh->lastFrameId;

    if (videoCh->frameCounter == 0) {
        IHS_SessionLog(channel->session, IHS_BaseLogLevelWarn, "No frames coming in, request keyframe");
        IHS_SessionChannelDataLost(channel);
        videoCh->waitingKeyFrame = true;
    }
    IHS_SessionLog(channel->session, IHS_BaseLogLevelInfo, "Video %.2f FPS", videoCh->frameCounter / 1.0);
    videoCh->frameCounter = 0;
    IHS_MutexUnlock(videoCh->stateMutex);
//    IHS_SessionChannelStatsSend(statsCh, k_EStreamStatsFrameEvents, (const ProtobufCMessage *) &message,
//                                IHS_PACKET_ID_NEXT);
    return 1000;
}