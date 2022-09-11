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
#include "partial_frames.h"

#include "crypto.h"
#include "endianness.h"
#include "session/channels/video/frame_h264.h"
#include "session/channels/ch_stats.h"
#include "protobuf/pb_utils.h"

typedef struct IHS_SessionChannelVideo {
    IHS_SessionChannelData base;
    IHS_StreamVideoConfig config;
    uint16_t expectedSequence;
    uint16_t lastFrameId;
    uint16_t frameCounter;
    bool waitingKeyFrame;
    bool frameFinished;
    uint16_t partialReserved1;
    IHS_VideoPartialFrames partialFrames;
    struct {
        IHS_Buffer data;
        IHS_StreamVideoFrameFlag flags;
    } frameBuffer;
    IHS_Timer *statsTimer;
    IHS_Mutex *stateMutex;
} IHS_SessionChannelVideo;


static void ChannelVideoInit(IHS_SessionChannel *channel, const void *config);

static void ChannelVideoDeinit(IHS_SessionChannel *channel);

static bool DataStart(IHS_SessionChannel *channel);

static void DataReceived(IHS_SessionChannel *channel, const IHS_SessionDataFrameHeader *header, IHS_Buffer *body);

static void DataStop(IHS_SessionChannel *channel);

static size_t VideoFrameHeaderParse(IHS_VideoFrameHeader *header, const uint8_t *data);

static void PreprocessAndSubmit(IHS_SessionChannel *channel, IHS_Buffer *data, const IHS_VideoFrameHeader *header);

static uint64_t ReportVideoStats(void *data);

static void FlushPending(IHS_SessionChannelVideo *videoCh);

static void AppendFrame(IHS_SessionChannelVideo *videoCh, IHS_Buffer *data, const IHS_VideoFrameHeader *header);

static void Callback(IHS_Session *session, IHS_Buffer *data, IHS_StreamVideoFrameFlag flags);

static void AddPartialFrame(IHS_SessionChannelVideo *channel, IHS_Buffer *data, const IHS_VideoFrameHeader *header);

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
    IHS_SessionChannelDataInit(channel, 2048);
}

static void ChannelVideoDeinit(IHS_SessionChannel *channel) {
    IHS_SessionChannelDataDeinit(channel);
    IHS_SessionChannelVideo *videoCh = (IHS_SessionChannelVideo *) channel;
    IHS_MutexDestroy(videoCh->stateMutex);
    if (videoCh->config.codecData) {
        free(videoCh->config.codecData);
    }
    IHS_BufferClear(&videoCh->frameBuffer.data, true);
    IHS_VideoPartialFramesClear(&videoCh->partialFrames);
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
        FlushPending(videoCh);
        videoCh->waitingKeyFrame = false;
        videoCh->expectedSequence = vhead.sequence;
        IHS_SessionLog(channel->session, IHS_BaseLogLevelInfo, "Coming keyframe");
    }
    if (videoCh->waitingKeyFrame) {
        // TODO: wait for a few hundreds of milliseconds after requesting keyframe. Then request again.
    }
    if (vhead.sequence != videoCh->expectedSequence) {
        if (!videoCh->waitingKeyFrame) {
            IHS_SessionLog(channel->session, IHS_BaseLogLevelWarn, "Unexpected video frame sequence %u (expect %u), "
                                                                   "request keyframe", vhead.sequence,
                           videoCh->expectedSequence);
            IHS_SessionChannelDataLost(channel);
        }
        videoCh->waitingKeyFrame = IHS_SessionPacketTimestamp();
        videoCh->expectedSequence = vhead.sequence;
    }
    videoCh->expectedSequence += 1;
    videoCh->lastFrameId = header->id;
    if (!videoCh->waitingKeyFrame) {
        if (vhead.flags & VideoFrameFlagEncrypted) {
            const IHS_SessionConfig *config = &channel->session->config;
            IHS_Buffer plain;
            IHS_BufferInit(&plain, 0, 0);
            IHS_BufferEnsureCapacityExact(&plain, body->size);
            size_t outLen = body->size;
            IHS_CryptoSymmetricDecryptWithIV(IHS_BufferPointer(body), body->size, EmptyIV, sizeof(EmptyIV),
                                             config->sessionKey, config->sessionKeyLen, IHS_BufferPointer(&plain),
                                             &outLen);
            plain.size = outLen;
            PreprocessAndSubmit(channel, &plain, &vhead);
            IHS_BufferClear(&plain, true);
        } else {
            PreprocessAndSubmit(channel, body, &vhead);
        }
        if (vhead.flags & VideoFrameFlagFrameFinish) {
            videoCh->frameCounter++;
        }
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


static void PreprocessAndSubmit(IHS_SessionChannel *channel, IHS_Buffer *data, const IHS_VideoFrameHeader *header) {
    IHS_SessionChannelVideo *videoCh = (IHS_SessionChannelVideo *) channel;
    AddPartialFrame(videoCh, data, header);

    IHS_VideoPartialFrame *partial = videoCh->partialFrames.head;
    do {
        bool middleOfFrame = false;
        if (partial != NULL && !videoCh->frameFinished) {
            middleOfFrame = true;
        }
        if (!middleOfFrame) {
            goto DealWithFrameHead;
        }
        IHS_VideoPartialFrame *next = partial->next;
        if (partial->header.reserved2 != 0) {
            if (partial->header.reserved1 != videoCh->partialReserved1) {
                DealWithFrameHead:
                if (videoCh->frameFinished) {
                    Callback(channel->session, &videoCh->frameBuffer.data, videoCh->frameBuffer.flags);
                    IHS_BufferClear(&videoCh->frameBuffer.data, false);
                    videoCh->frameBuffer.flags = 0;
                    videoCh->frameFinished = false;
                }
                break;
            }
            if (partial->header.flags & VideoFrameFlagReserved1Increment) {
                if (partial->header.flags & VideoFrameFlagFrameFinish) {
                    videoCh->partialReserved1 = 0;
                } else {
                    videoCh->partialReserved1 = partial->header.reserved2 + 1;
                    IHS_SessionLog(channel->session, IHS_BaseLogLevelInfo, "partialReserved1 incremented from %u to %u",
                                   partial->header.reserved2, videoCh->partialReserved1);
                }
            }
        }
        // append buffer
        AppendFrame(videoCh, &partial->data, &partial->header);
        if (partial->header.flags & VideoFrameFlagFrameFinish) {
            videoCh->frameFinished = true;
        }
        IHS_BufferClear(&partial->data, true);
        IHS_VideoPartialFramesRemove(&videoCh->partialFrames, partial);
        partial = next;
    } while (true);
}

static void AddPartialFrame(IHS_SessionChannelVideo *channel, IHS_Buffer *data, const IHS_VideoFrameHeader *header) {
    // Find first matching cur frame
    IHS_VideoPartialFrame *cur = NULL;
    IHS_VideoPartialFramesForEach (cur, &channel->partialFrames) {
        if (header->sequence == cur->header.sequence && header->reserved2 < cur->header.reserved1) {
            break;
        }
    }
    if (cur != NULL) {
        IHS_VideoPartialFramesInsertBefore(&channel->partialFrames, cur, header, data);
    } else {
        IHS_VideoPartialFramesAppend(&channel->partialFrames, header, data);
    }
}

static uint64_t ReportVideoStats(void *data) {
    IHS_SessionChannel *channel = data;
    IHS_SessionChannelVideo *videoCh = (IHS_SessionChannelVideo *) channel;
    IHS_MutexLock(videoCh->stateMutex);

    CFrameStatsListMsg message = CFRAME_STATS_LIST_MSG__INIT;
    message.data_type = k_EStreamingVideoData;
    message.latest_frame_id = videoCh->lastFrameId;

    IHS_SessionLog(channel->session, IHS_BaseLogLevelInfo, "Video %.2f FPS", videoCh->frameCounter / 1.0);
    videoCh->frameCounter = 0;
    IHS_MutexUnlock(videoCh->stateMutex);
    return 1000;
}

static void FlushPending(IHS_SessionChannelVideo *videoCh) {
    IHS_BufferClear(&videoCh->frameBuffer.data, 0);
    videoCh->frameBuffer.flags = 0;
    IHS_VideoPartialFramesClear(&videoCh->partialFrames);
    videoCh->partialReserved1 = 0;
}

static void AppendFrame(IHS_SessionChannelVideo *videoCh, IHS_Buffer *data, const IHS_VideoFrameHeader *header) {
    assert (videoCh->config.codec == IHS_StreamVideoCodecH264);
    IHS_SessionVideoFrameAppendH264(&videoCh->frameBuffer.data, IHS_BufferPointer(data), data->size, header);
    if (header->flags & VideoFrameFlagKeyFrame) {
        videoCh->frameBuffer.flags |= IHS_StreamVideoFrameKeyFrame;
    }
}

static void Callback(IHS_Session *session, IHS_Buffer *data, IHS_StreamVideoFrameFlag flags) {
    const IHS_StreamVideoCallbacks *callbacks = session->callbacks.video;
    void *context = session->callbackContexts.video;
    callbacks->submit(session, data, flags, context);
}