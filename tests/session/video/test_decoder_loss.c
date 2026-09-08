/*
 *  _____  _   _  _____  _  _  _
 * |_   _|| | | |/  ___|| |(_)| |     Steam
 *   | |  | |_| |\ `--. | | _ | |__     In-Home
 *   | |  |  _  | `--. \| || || '_ \      Streaming
 *  _| |_ | | | |/\__/ /| || || |_) |       Library
 *  \___/ \_| |_/\____/ |_||_||_.__/
 *
 * Copyright (c) 2026 Mariotaku <https://github.com/mariotaku>.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 */

/**
 * A decoder that rejects a frame must put the channel into keyframe wait, exactly as a sequence gap
 * does — otherwise the inter-frames that depend on the rejected one keep being handed to it.
 *
 * Frames go straight to the channel class's dataFrame hook, so this covers ch_data_video.c's
 * recovery logic without a socket. Two observables: submitted frames arrive through the video
 * callbacks, and every keyframe request (IHS_SessionChannelDataLost) queues one packet on the
 * session's send queue.
 */

#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "session/session_pri.h"
#include "session/channels/ch_data.h"
#include "session/channels/video/ch_data_video.h"
#include "endianness.h"
#include "ihs_queue.h"
#include "protobuf/pb_utils.h"

#include "test_session.h"

/** Mirrors session.c — the queue item layout is private there. */
typedef struct IHS_QueueItem {
    IHS_SessionPacket packet;
    bool retransmit;
} QueuedPacket;

#define VIDEO_HEADER_SIZE 7
#define MAX_SUBMITTED 8

static struct {
    uint8_t data[512];
    size_t len;
} submitted[MAX_SUBMITTED];

static size_t submittedCount = 0;

/** Result the next submit returns, then it falls back to OK. Lets a test fail one decode. */
static IHS_StreamVideoSubmitResult nextSubmitResult = IHS_StreamVideoSubmitOK;

static IHS_StreamVideoSubmitResult OnSubmit(IHS_Session *session, IHS_Buffer *data, IHS_StreamVideoFrameFlag flags,
                                            void *context) {
    (void) session;
    (void) context;
    (void) flags;
    assert(submittedCount < MAX_SUBMITTED);
    assert(data->size <= sizeof(submitted[0].data));
    memcpy(submitted[submittedCount].data, IHS_BufferPointer(data), data->size);
    submitted[submittedCount].len = data->size;
    submittedCount++;
    IHS_StreamVideoSubmitResult result = nextSubmitResult;
    nextSubmitResult = IHS_StreamVideoSubmitOK;
    return result;
}

static const IHS_StreamVideoCallbacks videoCallbacks = {
        .submit = OnSubmit,
};

/* ------------------------------------------------------------------ harness */

static IHS_Session *session = NULL;
static IHS_SessionChannel *channel = NULL;

/** Feed one data frame. `payload` is appended raw, so assembled output compares byte for byte. */
static void Feed(uint16_t frameId, uint32_t timestamp, uint16_t sequence, uint8_t flags,
                 uint16_t subFrameStart, uint16_t subFrameEnd, const uint8_t *payload, size_t payloadLen) {
    IHS_Buffer body = IHS_BUFFER_INIT(VIDEO_HEADER_SIZE + payloadLen, 4096);
    uint8_t head[VIDEO_HEADER_SIZE];
    size_t offset = 0;
    offset += IHS_WriteUInt16LE(&head[offset], sequence);
    head[offset++] = flags;
    offset += IHS_WriteUInt16LE(&head[offset], subFrameStart);
    offset += IHS_WriteUInt16LE(&head[offset], subFrameEnd);
    assert(offset == VIDEO_HEADER_SIZE);
    IHS_BufferAppendMem(&body, head, VIDEO_HEADER_SIZE);
    if (payloadLen > 0) {
        IHS_BufferAppendMem(&body, payload, payloadLen);
    }

    IHS_SessionDataFrameHeader header = {.id = frameId, .timestamp = timestamp};
    const IHS_SessionChannelDataClass *cls = (const IHS_SessionChannelDataClass *) channel->cls;
    cls->dataFrame(channel, &header, &body);
    IHS_BufferClear(&body, true);
}

/** Number of keyframe requests queued since the last call. */
static size_t TakeKeyframeRequests(void) {
    size_t count = 0;
    IHS_QueueItem *item;
    while ((item = IHS_QueuePoll(session->sendQueue)) != NULL) {
        QueuedPacket *queued = (QueuedPacket *) item;
        assert(IHS_BufferPointerAt(&queued->packet.body, 0)[0] == k_EStreamDataLost);
        IHS_SessionPacketClear(&queued->packet, true);
        IHS_QueueItemFree(item);
        count++;
    }
    return count;
}

static void Reset(void) {
    submittedCount = 0;
    nextSubmitResult = IHS_StreamVideoSubmitOK;
    TakeKeyframeRequests();
}

static void SetUp(void) {
    IHS_Init();
    session = IHS_TestSessionCreate();
    IHS_SessionSetVideoCallbacks(session, &videoCallbacks, NULL);
    CStartVideoDataMsg message = CSTART_VIDEO_DATA_MSG__INIT;
    message.channel = 3;
    PROTOBUF_C_SET_VALUE(message, codec, k_EStreamVideoCodecH264);
    PROTOBUF_C_SET_VALUE(message, width, 1280);
    PROTOBUF_C_SET_VALUE(message, height, 720);
    channel = IHS_SessionChannelDataVideoCreate(session, &message);
    assert(channel != NULL);
    Reset();
}

static void TearDown(void) {
    // The data channel spins up a worker thread at init, and its deinit asserts the thread has been
    // interrupted first. That is what the `stopped` hook does.
    channel->cls->stopped(channel);
    IHS_SessionChannelDestroy(channel);
    TakeKeyframeRequests();
    IHS_SessionDestroy(session);
    IHS_Quit();
}

/* ------------------------------------------------------------------ cases */

/**
 * The reference reaches this through a deferred reset: CMarvellAccel::BDecodeFrame @ 0x408adc asks
 * for ResetVideoDecoder @ 0x1fbe78, and HandlePendingResets @ 0x1fbed4 lands it on
 * CStreamDecoderVideo::StopDecoding @ 0x205574, which flushes pending data and sets the wait.
 */
static void test_decoder_reported_loss_requests_keyframe(void) {
    Reset();
    const uint8_t payload[] = {0x55, 0x66};
    const uint8_t after[] = {0x77, 0x88};

    Feed(1, 1000, 0, VideoFrameFlagKeyFrame | VideoFrameFlagSubFrameAdvance | VideoFrameFlagFrameFinish,
         0, 3, payload, sizeof(payload));
    assert(submittedCount == 1);
    assert(TakeKeyframeRequests() == 0);

    // Sequence is intact — the only thing wrong is that the decoder could not decode this frame.
    nextSubmitResult = IHS_StreamVideoSubmitReportLost;
    Feed(2, 2000, 1, VideoFrameFlagSubFrameAdvance | VideoFrameFlagFrameFinish, 0, 3, after, sizeof(after));
    assert(submittedCount == 2 && "the failing frame itself still reaches the decoder");
    // Two requests: the immediate one from SubmitFrame, mirroring FinalDecode @ 0x203844, and the
    // one HandlePendingResets sends once the reset has been applied.
    assert(TakeKeyframeRequests() == 2);

    // The wait is now armed, so the next in-sequence frame must be dropped rather than decoded.
    Feed(3, 3000, 2, VideoFrameFlagSubFrameAdvance | VideoFrameFlagFrameFinish, 0, 3, after, sizeof(after));
    assert(submittedCount == 2 && "frame after a decoder-reported loss must be dropped");

    // A keyframe clears the wait and streaming resumes.
    const uint8_t recovered[] = {0x99};
    Feed(4, 4000, 3, VideoFrameFlagKeyFrame | VideoFrameFlagSubFrameAdvance | VideoFrameFlagFrameFinish,
         0, 3, recovered, sizeof(recovered));
    assert(submittedCount == 3);
    assert(submitted[2].data[0] == 0x99);
}

int main(void) {
    SetUp();
    test_decoder_reported_loss_requests_keyframe();
    TearDown();
    printf("decoder loss tests OK\n");
    return 0;
}
