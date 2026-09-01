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
#include "frame_stats.h"

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
        uint16_t frameCounter;
        uint64_t waitingKeyFrame;
        uint64_t lastStatsTime;
        bool frameFinished;
        /**
         * Id of the last frame that finished assembly, as CStreamDecoderVideo+0x13e is advanced by
         * OnThink @ 0x205d18 — once per reassembled frame, before it is handed to the decoder.
         */
        uint16_t previousFrameId;
        /** False until the first frame completes, so the gap fill has a floor to work from. */
        bool previousFrameIdValid;
    } states;
    struct {
        uint16_t expectedSubFrameStart;
        /** Id of the frame currently in `buffer`, taken from the fragment that finished it. */
        uint16_t id;
        IHS_VideoPartialFrames partial;
        IHS_Buffer buffer;
        IHS_StreamVideoFrameFlag flags;
    } frame;
    IHS_VideoFrameStatsRing frameStats;
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
 * Report every frame id skipped over by a keyframe-driven reset, mirroring the loop at the head of
 * CStreamDecoderVideo::DecodeFrame @ 0x205638.
 * @param channel Channel instance
 * @param frameId Id of the arriving keyframe
 */
static void ReportSkippedFrames(IHS_SessionChannelVideo *channel, uint16_t frameId);

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
    // CStreamDecoderVideo's ctor @ 0x20528c seeds previousFrameId with 0xffff.
    videoCh->states.previousFrameId = 0xFFFF;
    IHS_VideoFrameStatsRingInit(&videoCh->frameStats);
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
    videoCh->statsTimer = IHS_TimerTaskStart(session->base.timers, ReportVideoStats, NULL, 1000, videoCh);

    return IHS_SessionSendControlMessage(session, k_EStreamControlVideoDecoderInfo,
                                         (const ProtobufCMessage *) &message);
}

static void DataReceived(IHS_SessionChannel *channel, const IHS_SessionDataFrameHeader *header, IHS_Buffer *body) {
    IHS_SessionChannelVideo *videoCh = (IHS_SessionChannelVideo *) channel;
    if (header == NULL) {
        // A data frame too short to carry a header has no frame id or timestamp, so there is
        // nothing to assemble it into. ch_data.c passes NULL for those.
        return;
    }
    IHS_VideoFrameHeader vhead;
    IHS_BufferOffsetBy(body, (int) VideoFrameHeaderParse(&vhead, IHS_BufferPointer(body)));

    IHS_MutexLock(videoCh->stateMutex);

    // Claim the frame's statistics slot before anything can discard it, as RecordFrameReceived
    // @ 0x1faac4 does off OnDataPacket.
    IHS_VideoFrameStatsReceived(&videoCh->frameStats, &(IHS_VideoFrameStatsInfo) {
            .frameId = header->id,
            .frameTimestamp = header->timestamp,
            .sendTimestamp = header->sendTimestamp,
            .recvTimestamp = header->recvTimestamp,
            .inputMark = header->inputMark,
            .hasInputMark = header->hasInputMark,
            .inputEventTimestamp = header->inputEventTimestamp,
            .inputSendTimestamp = header->inputSendTimestamp,
            .inputRecvTimestamp = header->inputRecvTimestamp,
    });

    if (vhead.flags & VideoFrameFlagKeyFrame) {
        // The reference reports the skipped ids before flushing, so the fragments about to be
        // dropped are still queued while the events are emitted (0x205638 precedes 0x2056b4).
        ReportSkippedFrames(videoCh, header->id);
        DiscardPending(videoCh);
        videoCh->states.waitingKeyFrame = 0;
        videoCh->states.expectedSequence = vhead.sequence;
        // One id back, so the keyframe itself stays outstanding and is closed out by assembly.
        videoCh->states.previousFrameId = header->id - 1;
        videoCh->states.previousFrameIdValid = true;
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
    if (videoCh->states.waitingKeyFrame > 0) {
        goto unlock;
    }
    if (vhead.flags & VideoFrameFlagEncrypted) {
        const IHS_SessionInfo *config = &channel->session->info;
        // Decrypt in place, like CStreamDecoderVideo does: the plaintext is never longer than the
        // ciphertext, and the partial frame takes ownership of `body` either way. Decrypting into a
        // scratch buffer would mean an alloc/free pair per packet on the 60 fps hot path.
        size_t outLen = body->size;
        int decryptRet = IHS_CryptoSymmetricDecryptWithIV(IHS_BufferPointer(body), body->size,
                                                          EmptyIV, sizeof(EmptyIV),
                                                          config->sessionKey, config->sessionKeyLen,
                                                          IHS_BufferPointer(body), &outLen);
        if (decryptRet != 0) {
            IHS_SessionLog(channel->session, IHS_LogLevelWarn, "Video",
                           "Failed to decrypt video frame: %d, request keyframe", decryptRet);
            IHS_SessionChannelDataLost(channel);
            videoCh->states.waitingKeyFrame = IHS_TimerNow();
            goto unlock;
        }
        body->size = outLen;
    }
    AddPartialFrame(videoCh, header->id, header->timestamp, &vhead, body);

    if (AssembleFrame(channel)) {
        IHS_StreamVideoSubmitResult result = SubmitFrame(channel, &videoCh->frame.buffer, videoCh->frame.flags);
        // ihslib hands the frame to the application and never hears about presentation, so an
        // accepted frame is the closest thing this layer has to IStreamPlayer's Displayed
        // (0x1856d4), and a decoder reporting it lost is FinalDecode's failure path (0x203824).
        IHS_VideoFrameStatsComplete(&videoCh->frameStats, videoCh->frame.id,
                                    result == IHS_StreamVideoSubmitOK ? k_EStreamFrameResultDisplayed
                                                                      : k_EStreamFrameResultDroppedDecodeCorrupt,
                                    IHS_SessionPacketTimestamp());
        IHS_BufferClear(&videoCh->frame.buffer, false);
        videoCh->frame.flags = 0;
        videoCh->states.frameFinished = false;
        // Defensive: the happy path resets this inside AssembleFrame when the last fragment carries
        // SubFrameAdvance|FrameFinish. A frame that finishes without that combo would otherwise leave
        // a stale counter and stall the next frame until a keyframe arrives.
        videoCh->frame.expectedSubFrameStart = 0;
        videoCh->states.frameCounter++;
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
            videoCh->frame.id = partial->frameId;
            // OnThink @ 0x205d18 advances the id here — once per reassembled frame, before the
            // decoder sees it, so a decode failure does not roll it back.
            videoCh->states.previousFrameId = partial->frameId;
            videoCh->states.previousFrameIdValid = true;
        }
        IHS_BufferClear(&partial->data, true);
        IHS_VideoPartialFramesRemove(&videoCh->frame.partial, partial);
        partial = next;
    }
    return videoCh->states.frameFinished;
}

static void AddPartialFrame(IHS_SessionChannelVideo *channel, uint16_t frameId, uint32_t timestamp,
                            const IHS_VideoFrameHeader *header, IHS_Buffer *data) {
    // Find the first pending fragment this one must be placed before.
    IHS_VideoPartialFrame *cur = NULL;
    IHS_VideoPartialFrame *tail = channel->frame.partial.tail;
    // Fast path for in-order arrival, which is the overwhelmingly common case and used to walk the
    // whole list to conclude nothing matched. Insertions only ever place a node before the first
    // same-frame node with a larger subFrameStart, so within a frame the list stays ascending and
    // the tail holds that frame's largest subFrameStart. If the predicate fails against the tail it
    // fails against every earlier node of the same frame too, so appending is provably identical to
    // the scan. Only when the tail belongs to a different frame can an older frame's group still be
    // pending further up, and then the full walk is still needed.
    if (tail != NULL && frameId == tail->frameId && header->subFrameEnd >= tail->header.subFrameStart) {
        cur = NULL;
    } else {
        IHS_VideoPartialFramesForEach (cur, &channel->frame.partial) {
            if (frameId == cur->frameId && header->subFrameEnd < cur->header.subFrameStart) {
                break;
            }
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

static void ReportSkippedFrames(IHS_SessionChannelVideo *channel, uint16_t frameId) {
    if (!channel->states.previousFrameIdValid) {
        // Nothing has completed yet, so there is no floor to count up from. The reference starts
        // from its 0xffff seed and reports every id below the first keyframe's; with a ring that is
        // still empty those events go nowhere, so skip the walk entirely.
        return;
    }
    uint16_t skipped = IHS_VideoFrameStatsGapSize(channel->states.previousFrameId, frameId);
    if (skipped > IHS_VIDEO_FRAME_STATS_RING_SIZE) {
        IHS_SessionLog(((IHS_SessionChannel *) channel)->session, IHS_LogLevelWarn, "Video",
                       "Keyframe %u is %u frames past %u, not reporting the gap", frameId, skipped,
                       channel->states.previousFrameId);
        return;
    }
    IHS_VideoFrameStatsReportSkipped(&channel->frameStats, channel->states.previousFrameId, frameId,
                                     IHS_SessionPacketTimestamp());
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
        IHS_SessionLog(session, IHS_LogLevelInfo, "Video", "Decoder reported frame lost.");
        IHS_SessionChannelDataLost(channel);
    } else if (result == IHS_StreamVideoSubmitError) {
        IHS_SessionLog(session, IHS_LogLevelError, "Video", "Decoder reported unrecoverable error.");
        IHS_SessionDisconnect(session);
    }
    return result;
}

/**
 * SendFrameEvents @ 0x1fb4f0, on the 1 Hz gate HandleStreaming @ 0x1f7b34 puts it behind. The
 * reference sends this on the stats channel with a leading k_EStreamStatsFrameEvents byte — not on
 * the control channel — and sends nothing at all when the drain found no frames.
 */
static uint64_t ReportVideoStats(int runCount, void *data) {
    (void) runCount;
    IHS_SessionChannel *channel = data;
    IHS_SessionChannelVideo *videoCh = (IHS_SessionChannelVideo *) channel;
    IHS_MutexLock(videoCh->stateMutex);

    IHS_FrameStatsAccumulator accumulator;
    IHS_FrameStatsAccumulatorInit(&accumulator);
    size_t drained = IHS_VideoFrameStatsDrain(&videoCh->frameStats, &accumulator, IHS_SessionPacketTimestamp());
    // The window ends at the last frame reported, which is the last one that made it to the
    // application — not the newest id received.
    int32_t latestFrameId = videoCh->frameStats.sendCursor;

    uint64_t now = IHS_TimerNow();
    uint64_t elapsedMs = now - videoCh->states.lastStatsTime;
    double fps = elapsedMs > 0 ? (videoCh->states.frameCounter * 1000.0) / (double) elapsedMs : 0.0;
    IHS_SessionLog(channel->session, IHS_LogLevelVerbose, "Video", "%.2f FPS", fps);
    videoCh->states.frameCounter = 0;
    videoCh->states.lastStatsTime = now;
    IHS_MutexUnlock(videoCh->stateMutex);

    if (drained == 0) {
        return 1000;
    }
    CFrameStatsListMsg message = CFRAME_STATS_LIST_MSG__INIT;
    message.data_type = k_EStreamingVideoData;
    message.latest_frame_id = latestFrameId;
    // Per-frame rows are the reference's sendFullFrameStats mode (CStreamClient+0x160), which is
    // off unless the on-screen stats overlay turns it on, so only the accumulated values go out.
    // Unlike the reference there are no network samples to add — RTT, bitrates and packet loss have
    // no equivalent here yet — so a tick that accumulated nothing has nothing worth sending.
    if (IHS_FrameStatsAccumulatorFill(&accumulator, &message)) {
        IHS_SessionChannel *statsChannel = IHS_SessionChannelFor(channel->session, IHS_SessionChannelIdStats);
        if (statsChannel != NULL) {
            IHS_SessionChannelStatsSend(statsChannel, k_EStreamStatsFrameEvents, (const ProtobufCMessage *) &message,
                                        IHS_PACKET_ID_NEXT);
        }
        IHS_FrameStatsListClear(&message);
    }
    return 1000;
}
