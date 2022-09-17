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

#include "session/channels/ch_data.h"
#include "ch_data_video.h"
#include "partial_frames.h"

#include "ihs_timer.h"

#include "crypto.h"
#include "endianness.h"
#include "session/session_pri.h"
#include "session/channels/ch_stats.h"
#include "protobuf/pb_utils.h"

#include "frame_h264.h"
#include "frame_hevc.h"

typedef struct IHS_SessionChannelVideo {
    IHS_SessionChannelData base;
    IHS_StreamVideoConfig config;
    struct {
        uint16_t expectedSequence;
        uint16_t lastFrameId;
        uint16_t frameCounter;
        uint64_t waitingKeyFrame;
        bool frameFinished;
    } states;
    struct {
        uint16_t reserved1;
        IHS_VideoPartialFrames partial;
        IHS_Buffer buffer;
        IHS_StreamVideoFrameFlag flags;
    } frame;
    IHS_Timer *statsTimer;
    IHS_Mutex *stateMutex;
} IHS_SessionChannelVideo;


static void ChannelVideoInit(IHS_SessionChannel *channel, const void *config);

static void ChannelVideoDeinit(IHS_SessionChannel *channel);

static bool DataStart(IHS_SessionChannel *channel);

static void DataReceived(IHS_SessionChannel *channel, const IHS_SessionDataFrameHeader *header, IHS_Buffer *body);

static void DataStop(IHS_SessionChannel *channel);

static size_t VideoFrameHeaderParse(IHS_VideoFrameHeader *header, const uint8_t *data);

/**
 * Assemble one frame in the partial frames list
 * @param channel Channel instance
 * @return true if the frame is ready
 */
static bool AssembleFrame(IHS_SessionChannel *channel);

static void AppendToFrameBuffer(IHS_SessionChannelVideo *channel, const IHS_Buffer *data,
                                const IHS_VideoFrameHeader *header);

static void SubmitFrame(IHS_Session *session, IHS_Buffer *data, IHS_StreamVideoFrameFlag flags);

static uint64_t ReportVideoStats(void *data);

/**
 * Append one data frame into partial video frames list
 * @param channel Channel instance
 * @param data Data frame body
 * @param header Data frame header
 */
static void AddPartialFrame(IHS_SessionChannelVideo *channel, IHS_Buffer *data, const IHS_VideoFrameHeader *header);

/**
 * Clear partial video frames and not yet assembled frame data
 * @param channel
 */
static void DiscardPending(IHS_SessionChannelVideo *channel);

static const IHS_SessionChannelDataClass ChannelClass = {
        {
                .init = ChannelVideoInit,
                .deinit = ChannelVideoDeinit,
                .received = IHS_SessionChannelDataReceived,
                .stopped = IHS_SessionChannelDataStopped,
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
        videoCh->config.codecData = malloc(videoCh->config.codecDataLen);
        memcpy(videoCh->config.codecData, message->codec_data.data, message->codec_data.len);
    }
    videoCh->stateMutex = IHS_MutexCreate();
    IHS_SessionChannelDataInit(channel, 2048);
}

static void ChannelVideoDeinit(IHS_SessionChannel *channel) {
    IHS_SessionChannelDataDeinit(channel);
    IHS_SessionChannelVideo *videoCh = (IHS_SessionChannelVideo *) channel;
    IHS_MutexDestroy(videoCh->stateMutex);
    if (videoCh->config.codecData) {
        free(videoCh->config.codecData);
    }
    IHS_BufferClear(&videoCh->frame.buffer, true);
    IHS_VideoPartialFramesClear(&videoCh->frame.partial);
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
                                         (const ProtobufCMessage *) &message);
}

static void DataReceived(IHS_SessionChannel *channel, const IHS_SessionDataFrameHeader *header, IHS_Buffer *body) {
    IHS_SessionChannelVideo *videoCh = (IHS_SessionChannelVideo *) channel;
    IHS_VideoFrameHeader vhead;
    IHS_BufferOffsetBy(body, (int) VideoFrameHeaderParse(&vhead, IHS_BufferPointer(body)));

    IHS_MutexLock(videoCh->stateMutex);

    if (vhead.flags & VideoFrameFlagKeyFrame) {
        DiscardPending(videoCh);
        videoCh->states.waitingKeyFrame = 0;
        videoCh->states.expectedSequence = vhead.sequence;
        IHS_SessionLog(channel->session, IHS_LogLevelDebug, "Video", "Coming keyframe");
    }
    if (videoCh->states.waitingKeyFrame > 0) {
        // Wait for 200ms after requesting keyframe. Then request again.
        uint64_t now = IHS_TimerNow();
        if (now - videoCh->states.waitingKeyFrame >= 200) {
            IHS_SessionLog(channel->session, IHS_LogLevelWarn, "Video",
                           "Keyframe wait timeout, re-request keyframe", vhead.sequence,
                           videoCh->states.expectedSequence);
            IHS_SessionChannelDataLost(channel);
            videoCh->states.waitingKeyFrame = IHS_TimerNow();
        }
    } else if (vhead.sequence != videoCh->states.expectedSequence) {
        if (videoCh->states.waitingKeyFrame == 0) {
            IHS_SessionLog(channel->session, IHS_LogLevelWarn, "Video",
                           "Unexpected video frame sequence %u (expect %u), request keyframe", vhead.sequence,
                           videoCh->states.expectedSequence);
            IHS_SessionChannelDataLost(channel);
            videoCh->states.waitingKeyFrame = IHS_TimerNow();
        }
    }
    videoCh->states.expectedSequence = vhead.sequence + 1;
    videoCh->states.lastFrameId = header->id;
    if (videoCh->states.waitingKeyFrame > 0) {
        goto unlock;
    }
    if (vhead.flags & VideoFrameFlagEncrypted) {
        const IHS_SessionInfo *config = &channel->session->info;
        IHS_Buffer plain;
        IHS_BufferInit(&plain, 0, 0);
        IHS_BufferEnsureMaxSizeExact(&plain, body->size);
        size_t outLen = body->size;
        IHS_CryptoSymmetricDecryptWithIV(IHS_BufferPointer(body), body->size, EmptyIV, sizeof(EmptyIV),
                                         config->sessionKey, config->sessionKeyLen, IHS_BufferPointer(&plain),
                                         &outLen);
        plain.size = outLen;
        AddPartialFrame(videoCh, &plain, &vhead);
        IHS_BufferClear(&plain, true);
    } else {
        AddPartialFrame(videoCh, body, &vhead);
    }

    if (AssembleFrame(channel)) {
        SubmitFrame(channel->session, &videoCh->frame.buffer, videoCh->frame.flags);
        IHS_BufferClear(&videoCh->frame.buffer, false);
        videoCh->frame.flags = 0;
        videoCh->states.frameFinished = false;
        videoCh->states.frameCounter++;
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

static size_t VideoFrameHeaderParse(IHS_VideoFrameHeader *header, const uint8_t *data) {
    size_t offset = 0;
    offset += IHS_ReadUInt16LE(&data[offset], &header->sequence);
    header->flags = data[offset++];
    offset += IHS_ReadUInt16LE(&data[offset], &header->reserved1);
    offset += IHS_ReadUInt16LE(&data[offset], &header->reserved2);
    return offset;
}

static bool AssembleFrame(IHS_SessionChannel *channel) {
    IHS_SessionChannelVideo *videoCh = (IHS_SessionChannelVideo *) channel;

    IHS_VideoPartialFrame *partial = videoCh->frame.partial.head;
    while (partial != NULL && !videoCh->states.frameFinished) {
        IHS_VideoPartialFrame *next = partial->next;
        if (partial->header.reserved2 != 0) {
            if (partial->header.reserved1 != videoCh->frame.reserved1) {
                break;
            }
            if (partial->header.flags & VideoFrameFlagReserved1Increment) {
                if (partial->header.flags & VideoFrameFlagFrameFinish) {
                    videoCh->frame.reserved1 = 0;
                } else {
                    videoCh->frame.reserved1 = partial->header.reserved2 + 1;
                }
            }
        }
        // append buffer
        AppendToFrameBuffer(videoCh, &partial->data, &partial->header);
        if (partial->header.flags & VideoFrameFlagFrameFinish) {
            videoCh->states.frameFinished = true;
        }
        IHS_BufferClear(&partial->data, true);
        IHS_VideoPartialFramesRemove(&videoCh->frame.partial, partial);
        partial = next;
    }
    return videoCh->states.frameFinished;
}

static void AddPartialFrame(IHS_SessionChannelVideo *channel, IHS_Buffer *data, const IHS_VideoFrameHeader *header) {
    // Find first matching cur frame
    IHS_VideoPartialFrame *cur = NULL;
    IHS_VideoPartialFramesForEach (cur, &channel->frame.partial) {
        if (header->sequence == cur->header.sequence && header->reserved2 < cur->header.reserved1) {
            break;
        }
    }
    if (cur != NULL) {
        IHS_VideoPartialFramesInsertBefore(&channel->frame.partial, cur, header, data);
    } else {
        IHS_VideoPartialFramesAppend(&channel->frame.partial, header, data);
    }
}

static void DiscardPending(IHS_SessionChannelVideo *channel) {
    IHS_BufferClear(&channel->frame.buffer, 0);
    IHS_VideoPartialFramesClear(&channel->frame.partial);
    channel->frame.flags = 0;
    channel->frame.reserved1 = 0;
}

static void AppendToFrameBuffer(IHS_SessionChannelVideo *channel, const IHS_Buffer *data,
                                const IHS_VideoFrameHeader *header) {
    switch (channel->config.codec) {
        case IHS_StreamVideoCodecH264:
            IHS_SessionVideoFrameAppendH264(&channel->frame.buffer, IHS_BufferPointer(data), data->size, header);
            break;
        case IHS_StreamVideoCodecHEVC:
            IHS_SessionVideoFrameAppendHEVC(&channel->frame.buffer, IHS_BufferPointer(data), data->size, header);
            break;
        default: {
            IHS_SessionLog(((IHS_SessionChannel *) channel)->session, IHS_LogLevelFatal, "Video",
                           "Unsupported codec %u", channel->config.codec);
            abort();
        }
    }
    if (header->flags & VideoFrameFlagKeyFrame) {
        channel->frame.flags |= IHS_StreamVideoFrameKeyFrame;
    }
}

static void SubmitFrame(IHS_Session *session, IHS_Buffer *data, IHS_StreamVideoFrameFlag flags) {
    const IHS_StreamVideoCallbacks *callbacks = session->callbacks.video;
    if (callbacks == NULL || callbacks->submit == NULL) {
        return;
    }
    void *context = session->callbackContexts.video;
    callbacks->submit(session, data, flags, context);
}

static uint64_t ReportVideoStats(void *data) {
    IHS_SessionChannel *channel = data;
    IHS_SessionChannelVideo *videoCh = (IHS_SessionChannelVideo *) channel;
    IHS_MutexLock(videoCh->stateMutex);

    CFrameStatsListMsg message = CFRAME_STATS_LIST_MSG__INIT;
    message.data_type = k_EStreamingVideoData;
    message.latest_frame_id = videoCh->states.lastFrameId;

    IHS_SessionLog(channel->session, IHS_LogLevelInfo, "Video", "%.2f FPS", videoCh->states.frameCounter / 1.0);
    videoCh->states.frameCounter = 0;
    IHS_MutexUnlock(videoCh->stateMutex);
    return 1000;
}