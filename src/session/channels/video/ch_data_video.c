/*
 *  _____  _   _  _____  _  _  _     
 * |_   _|| | | |/  ___|| |(_)| |     Steam    
 *   | |  | |_| |\ `--. | | _ | |__     In-Home
 *   | |  |  _  | `--. \| || || '_ \      Streaming
 *  _| |_ | | | |/\__/ /| || || |_) |       Library
 *  \___/ \_| |_/\____/ |_||_||_.__/
 *
 * Copyright (c) 2022 Mariotaku <https://github.com/mariotaku>.
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
#include "session/frame_stats.h"
#include "session/channels/ch_stats.h"
#include "protobuf/pb_utils.h"
#include "session/packet.h"

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
        uint64_t lastStatsTime;
        bool frameStarted;
        bool frameFinished;
    } states;
    struct {
        uint16_t expectedSubFrameStart;
        IHS_VideoPartialFrames partial;
        IHS_Buffer buffer;
        IHS_StreamVideoFrameFlag flags;
    } frame;
    IHS_TimerTask *statsTimer;
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

/**
 * Hand one assembled frame to the application's decoder.
 * @return What the decoder made of it, for the caller to act on
 */
static IHS_StreamVideoSubmitResult SubmitFrame(IHS_SessionChannel *channel, IHS_Buffer *data,
                                               IHS_StreamVideoFrameFlag flags);

static uint64_t ReportVideoStats(int runCount, void *data);

/**
 * Append one data frame into partial video frames list
 * @param channel Channel instance
 * @param data Data frame body
 * @param header Data frame header
 */
static void AddPartialFrame(IHS_SessionChannelVideo *channel, uint16_t frameId, uint32_t timestamp,
                            const IHS_VideoFrameHeader *header, IHS_Buffer *data);

/**
 * If the oldest pending fragment is older than ~150 ms of stream time, drop the assembly state and
 * request a keyframe. Mirrors CStreamDecoderVideo::CheckOverflow.
 * @param channel Channel instance
 */
static void CheckPartialOverflow(IHS_SessionChannel *channel);

/**
 * Clear partial video frames and not yet assembled frame data
 * @param channel
 */
static void DiscardPending(IHS_SessionChannelVideo *channel);

/**
 * Apply a decoder reset requested by SubmitFrame, mirroring CStreamClient::HandlePendingResets
 * @ 0x1fbed4. Must be called with stateMutex held.
 * @param channel Channel instance
 */
/**
 * Drop the assembly state and wait for a keyframe after the decoder rejected a frame, mirroring
 * CStreamDecoderVideo::StopDecoding @ 0x205574. Must be called with stateMutex held.
 * @param channel Channel instance
 */
static void ApplyDecoderReset(IHS_SessionChannel *channel);

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
    IHS_BufferInit(&videoCh->frame.buffer, 128 * 1024/*128KB*/, 2048 * 1024/*2MB*/);
    IHS_VideoPartialFramesInit(&videoCh->frame.partial);
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

    videoCh->states.lastStatsTime = IHS_TimerNow();
    videoCh->statsTimer = IHS_TimerTaskStart(session->timers, ReportVideoStats, NULL, 1000, videoCh);

    return IHS_SessionSendControlMessage(session, k_EStreamControlVideoDecoderInfo,
                                         (const ProtobufCMessage *) &message);
}

static void DataReceived(IHS_SessionChannel *channel, const IHS_SessionDataFrameHeader *header, IHS_Buffer *body) {
    IHS_SessionChannelVideo *videoCh = (IHS_SessionChannelVideo *) channel;
    IHS_VideoFrameHeader vhead;
    IHS_BufferOffsetBy(body, (int) VideoFrameHeaderParse(&vhead, IHS_BufferPointer(body)));

    /* Seed events 5 (FrameEventStart) / 12 (FrameEventSend) / 13 (FrameEventRecv)
     * for this frame in the stats aggregator. Done outside the videoCh mutex so
     * the aggregator's own lock orders independently. Mirrors Steam's
     * RecordFrameReceived (0x001faac4): the partial-frame header carries the
     * sender's frame timestamp; we treat it as both event 5 and event 12 (we
     * do not yet maintain a separate sender-send timestamp), and stamp event
     * 13 with the local recv clock. */
    if (channel->session->frameStats != NULL) {
        IHS_FrameStatsRecordReceived(channel->session->frameStats, header->id,
                                     header->timestamp, header->timestamp,
                                     IHS_SessionPacketTimestamp(), 0 /* frameSize unknown until assembly */,
                                     0 /* inputMark — not yet on the wire here */);
    }

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
            IHS_SessionLog(channel->session, IHS_LogLevelWarn, "Video", "Keyframe wait timeout, re-request keyframe");
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
        int decryptRet = IHS_CryptoSymmetricDecryptWithIV(IHS_BufferPointer(body), body->size,
                                                          EmptyIV, sizeof(EmptyIV),
                                                          config->sessionKey, config->sessionKeyLen,
                                                          IHS_BufferPointer(&plain), &outLen);
        if (decryptRet != 0) {
            IHS_SessionLog(channel->session, IHS_LogLevelWarn, "Video",
                           "Failed to decrypt video frame: %d, request keyframe", decryptRet);
            IHS_BufferClear(&plain, true);
            IHS_SessionChannelDataLost(channel);
            videoCh->states.waitingKeyFrame = IHS_TimerNow();
            goto unlock;
        }
        plain.size = outLen;
        AddPartialFrame(videoCh, header->id, header->timestamp, &vhead, &plain);
        IHS_BufferClear(&plain, true);
    } else {
        AddPartialFrame(videoCh, header->id, header->timestamp, &vhead, body);
    }

    if (AssembleFrame(channel)) {
        IHS_StreamVideoSubmitResult result = SubmitFrame(channel, &videoCh->frame.buffer, videoCh->frame.flags);
        IHS_BufferClear(&videoCh->frame.buffer, false);
        videoCh->frame.flags = 0;
        videoCh->states.frameFinished = false;
        videoCh->states.frameStarted = false;
        videoCh->states.frameCounter++;
        if (result == IHS_StreamVideoSubmitReportLost) {
            // After the buffer cleanup, so the reset sees the same state a fresh frame would.
            ApplyDecoderReset(channel);
        }
    }
    CheckPartialOverflow(channel);
    unlock:
    IHS_MutexUnlock(videoCh->stateMutex);
}

static void DataStop(IHS_SessionChannel *channel) {
    IHS_Session *session = channel->session;
    IHS_SessionChannelVideo *videoCh = (IHS_SessionChannelVideo *) channel;
    if (videoCh->statsTimer != NULL) {
        IHS_TimerTaskStop(videoCh->statsTimer);
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
    offset += IHS_ReadUInt16LE(&data[offset], &header->subFrameStart);
    offset += IHS_ReadUInt16LE(&data[offset], &header->subFrameEnd);
    return offset;
}

static bool AssembleFrame(IHS_SessionChannel *channel) {
    IHS_SessionChannelVideo *videoCh = (IHS_SessionChannelVideo *) channel;

    IHS_VideoPartialFrame *partial = videoCh->frame.partial.head;
    while (partial != NULL && !videoCh->states.frameFinished) {
        IHS_VideoPartialFrame *next = partial->next;
        if (partial->header.subFrameEnd != 0) {
            if (partial->header.subFrameStart != videoCh->frame.expectedSubFrameStart) {
                break;
            }
            if (partial->header.flags & VideoFrameFlagSubFrameAdvance) {
                if (partial->header.flags & VideoFrameFlagFrameFinish) {
                    videoCh->frame.expectedSubFrameStart = 0;
                } else {
                    videoCh->frame.expectedSubFrameStart = partial->header.subFrameEnd + 1;
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

static void AddPartialFrame(IHS_SessionChannelVideo *channel, uint16_t frameId, uint32_t timestamp,
                            const IHS_VideoFrameHeader *header, IHS_Buffer *data) {
    // Find reset matching cur frame
    IHS_VideoPartialFrame *cur = NULL;
    IHS_VideoPartialFramesForEach (cur, &channel->frame.partial) {
        if (frameId == cur->frameId && header->subFrameEnd < cur->header.subFrameStart) {
            break;
        }
    }
    IHS_VideoPartialFrame *inserted;
    if (cur != NULL) {
        inserted = IHS_VideoPartialFramesInsertBefore(&channel->frame.partial, cur, frameId, header, data);
    } else {
        inserted = IHS_VideoPartialFramesAppend(&channel->frame.partial, frameId, header, data);
    }
    inserted->timestamp = timestamp;
}

static void CheckPartialOverflow(IHS_SessionChannel *channel) {
    IHS_SessionChannelVideo *videoCh = (IHS_SessionChannelVideo *) channel;
    if (videoCh->states.waitingKeyFrame > 0) {
        return;
    }
    IHS_VideoPartialFrame *head = videoCh->frame.partial.head;
    IHS_VideoPartialFrame *tail = videoCh->frame.partial.tail;
    if (head == NULL || head == tail) {
        return;
    }
    // 150 ms in 1/65536-second units. Span between oldest and newest pending fragment.
    const uint32_t overflowSpan = (uint32_t) (150 * 65536 / 1000);
    uint32_t span = tail->timestamp - head->timestamp;
    if (span <= overflowSpan) {
        return;
    }
    IHS_SessionLog(channel->session, IHS_LogLevelWarn, "Video",
                   "Partial frames stalled for %u ms, request keyframe", span * 1000 / 65536);
    IHS_SessionChannelDataLost(channel);
    videoCh->states.waitingKeyFrame = IHS_TimerNow();
}

static void ApplyDecoderReset(IHS_SessionChannel *channel) {
    IHS_SessionChannelVideo *videoCh = (IHS_SessionChannelVideo *) channel;
    // A reset lands on CStreamDecoderVideo::StopDecoding @ 0x205574, which flushes every queued
    // fragment and the half-assembled frame (FlushPendingData @ 0x206134), clears the assembly
    // state and waits for a keyframe. expectedSequence is deliberately untouched — the reference
    // does not reset it across a decoder reset.
    IHS_SessionLog(channel->session, IHS_LogLevelWarn, "Video", "Decoder reset, request keyframe");
    DiscardPending(videoCh);
    videoCh->states.frameFinished = false;
    videoCh->states.frameStarted = false;
    videoCh->states.waitingKeyFrame = IHS_TimerNow();
    // Second request of the pair. HandlePendingResets sends one once the reset completes, on top of
    // the one FinalDecode @ 0x203844 already sent from the decoder thread; this is the one that
    // arms the retry window above, so it must come after waitingKeyFrame is set.
    IHS_SessionChannelDataLost(channel);
}

static void DiscardPending(IHS_SessionChannelVideo *channel) {
    IHS_BufferClear(&channel->frame.buffer, 0);
    size_t clearedCount = IHS_VideoPartialFramesClear(&channel->frame.partial);
    if (clearedCount > 0) {
        IHS_SessionLog(((IHS_SessionChannel *) channel)->session, IHS_LogLevelWarn, "Video",
                       "%u partial frames was cleared", clearedCount);
    }
    channel->frame.flags = 0;
    channel->frame.expectedSubFrameStart = 0;
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

static IHS_StreamVideoSubmitResult SubmitFrame(IHS_SessionChannel *channel, IHS_Buffer *data,
                                               IHS_StreamVideoFrameFlag flags) {
    IHS_Session *session = channel->session;
    const IHS_StreamVideoCallbacks *callbacks = session->callbacks.video;
    if (callbacks == NULL || callbacks->submit == NULL) {
        return IHS_StreamVideoSubmitOK;
    }
    void *context = session->callbackContexts.video;
    IHS_StreamVideoSubmitResult result = callbacks->submit(session, data, flags, context);
    if (result == IHS_StreamVideoSubmitReportLost) {
        // The immediate request of the pair, as FinalDecode sends one at 0x203844 before the reset
        // it asked for has run. The caller applies the reset, which sends the second.
        IHS_SessionLog(session, IHS_LogLevelInfo, "Video", "Decoder reported frame lost.");
        IHS_SessionChannelDataLost(channel);
    } else if (result == IHS_StreamVideoSubmitError) {
        IHS_SessionLog(session, IHS_LogLevelError, "Video", "Decoder reported unrecoverable error.");
        IHS_SessionDisconnect(session);
    }
    return result;
}

static uint64_t ReportVideoStats(int runCount, void *data) {
    (void) runCount;
    IHS_SessionChannel *channel = data;
    IHS_SessionChannelVideo *videoCh = (IHS_SessionChannelVideo *) channel;
    IHS_Session *session = channel->session;

    /* FPS log lives entirely under videoCh's lock since it touches frameCounter
     * which is incremented inside the receive critical section. */
    IHS_MutexLock(videoCh->stateMutex);
    uint64_t now = IHS_TimerNow();
    uint64_t elapsedMs = now - videoCh->states.lastStatsTime;
    double fps = elapsedMs > 0 ? (videoCh->states.frameCounter * 1000.0) / (double) elapsedMs : 0.0;
    IHS_SessionLog(channel->session, IHS_LogLevelVerbose, "Video", "%.2f FPS", fps);
    videoCh->states.frameCounter = 0;
    videoCh->states.lastStatsTime = now;
    IHS_MutexUnlock(videoCh->stateMutex);

    /* Drain the per-frame stats aggregator into the accumulator and ship one
     * CFrameStatsListMsg over the stats channel. Matches Steam's
     * CStreamClient::SendFrameEvents (0x001fb4f0) — 1 Hz, video only, packet
     * suppressed when nothing was folded. */
    if (session->frameStats == NULL) {
        return 1000;
    }
    uint16_t latest = 0;
    size_t folded = IHS_FrameStatsAggregatorDrain(session->frameStats, &latest);
    if (folded == 0) {
        return 1000;
    }
    IHS_SessionChannel *stats = IHS_SessionChannelFor(session, IHS_SessionChannelIdStats);
    if (stats == NULL) {
        return 1000;
    }

    CFrameStatsListMsg message = CFRAME_STATS_LIST_MSG__INIT;
    message.data_type = k_EStreamingVideoData;
    message.latest_frame_id = latest;

    /* Pull the accumulator snapshot under the aggregator's lock by walking it
     * directly; the drain already populated it. Build the repeated
     * accumulated_stats field with one row per non-empty slot. */
    IHS_FrameStatsAccumulator *accum = &session->frameStats->accumulator;
    CFrameStatAccumulatedValue accumRows[IHS_FRAME_STATS_ACCUM_SLOTS];
    CFrameStatAccumulatedValue *accumPtrs[IHS_FRAME_STATS_ACCUM_SLOTS];
    size_t rowCount = 0;
    for (int i = 0; i < IHS_FRAME_STATS_ACCUM_SLOTS; i++) {
        if (accum->slots[i].count == 0) {
            continue;
        }
        CFrameStatAccumulatedValue *row = &accumRows[rowCount];
        cframe_stat_accumulated_value__init(row);
        row->stat_type = (EFrameAccumulatedStat) i;
        row->count = (int32_t) accum->slots[i].count;
        double average = accum->slots[i].sum / accum->slots[i].count;
        row->average = (float) average;
        /* Stddev = sqrt(E[X^2] - E[X]^2). Suppress when ≤ 0 (single sample or
         * negative-from-FP-roundoff). Matches Steam's set_stddev guard. */
        if (accum->slots[i].count > 1) {
            double meanSq = accum->slots[i].sumSquares / accum->slots[i].count;
            double variance = meanSq - average * average;
            if (variance > 0) {
                row->has_stddev = 1;
                row->stddev = (float) sqrt(variance);
            }
        }
        accumPtrs[rowCount] = row;
        rowCount++;
    }
    message.n_accumulated_stats = rowCount;
    message.accumulated_stats = accumPtrs;

    IHS_SessionChannelStatsSend(stats, k_EStreamStatsFrameEvents, (const ProtobufCMessage *) &message,
                                IHS_PACKET_ID_NEXT);
    IHS_FrameStatsAccumulatorReset(accum);
    return 1000;
}