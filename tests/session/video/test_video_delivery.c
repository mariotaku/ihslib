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
 * Exercises the video data channel under imperfect delivery: fragments arriving out of order, lost
 * frames, and jitter that stalls assembly. Frames are handed straight to the channel class's
 * dataFrame hook, so this covers ch_data_video.c's assembly and recovery logic without needing a
 * socket or the packet reassembly underneath it.
 *
 * Two observables: submitted frames arrive through the video callbacks, and every keyframe request
 * (IHS_SessionChannelDataLost) queues one packet on the session's send queue.
 */

#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "session/session_pri.h"
#include "session/channels/ch_data.h"
#include "session/channels/video/ch_data_video.h"
#include "crypto.h"
#include "endianness.h"
#include "ihs_queue.h"
#include "protobuf/pb_utils.h"

#include "test_session.h"

/** Mirrors session.c — see the note in test_control_hid.c. */
typedef struct IHS_QueueItem {
    IHS_SessionPacket packet;
    bool retransmit;
} QueuedPacket;

#define VIDEO_HEADER_SIZE 7
#define MAX_SUBMITTED 8

/** 150 ms in the wire's 1/65536-second units, matching CheckPartialOverflow. */
#define OVERFLOW_SPAN (150 * 65536 / 1000)

static const uint8_t EmptyIV[16] = {0};

static struct {
    uint8_t data[512];
    size_t len;
    IHS_StreamVideoFrameFlag flags;
} submitted[MAX_SUBMITTED];

static size_t submittedCount = 0;

static IHS_StreamVideoSubmitResult OnSubmit(IHS_Session *session, IHS_Buffer *data, IHS_StreamVideoFrameFlag flags,
                                            void *context) {
    (void) session;
    (void) context;
    assert(submittedCount < MAX_SUBMITTED);
    assert(data->size <= sizeof(submitted[0].data));
    memcpy(submitted[submittedCount].data, IHS_BufferPointer(data), data->size);
    submitted[submittedCount].len = data->size;
    submitted[submittedCount].flags = flags;
    submittedCount++;
    return IHS_StreamVideoSubmitOK;
}

static const IHS_StreamVideoCallbacks videoCallbacks = {
        .submit = OnSubmit,
};

/* ------------------------------------------------------------------ harness */

static IHS_Session *session = NULL;
static IHS_SessionChannel *channel = NULL;

/**
 * Feed one data frame. `payload` is appended raw — no NeedEscape/NeedStartSequence — so assembled
 * output can be compared byte for byte.
 */
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
    TakeKeyframeRequests();
}

static void SetUp(void) {
    IHS_Init();
    session = IHS_TestSessionCreate();
    IHS_SessionSetVideoCallbacks(session, &videoCallbacks, NULL);
    CStartVideoDataMsg message = CSTART_VIDEO_DATA_MSG__INIT;
    // Any free data-channel id; nothing in this test depends on the specific value.
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

/** Baseline: one keyframe in one fragment comes straight out. */
static void test_single_frame(void) {
    Reset();
    const uint8_t payload[] = {0x11, 0x22, 0x33, 0x44};
    Feed(1, 1000, 0, VideoFrameFlagKeyFrame | VideoFrameFlagSubFrameAdvance | VideoFrameFlagFrameFinish,
         0, 3, payload, sizeof(payload));
    assert(submittedCount == 1);
    assert(submitted[0].len == sizeof(payload));
    assert(memcmp(submitted[0].data, payload, sizeof(payload)) == 0);
    assert(submitted[0].flags & IHS_StreamVideoFrameKeyFrame);
    assert(TakeKeyframeRequests() == 0);
}

/**
 * Fragments arriving newest-first must still assemble in subFrameStart order. The tail fragment
 * cannot be consumed while the head is missing, so nothing is submitted until the gap is filled.
 */
static void test_out_of_order_fragments(void) {
    Reset();
    const uint8_t tail[] = {0xAA, 0xBB};
    const uint8_t head[] = {0x01, 0x02};

    // Tail first: subFrameStart 10 does not match the expected 0, so assembly stalls.
    Feed(1, 1000, 0, VideoFrameFlagKeyFrame | VideoFrameFlagSubFrameAdvance | VideoFrameFlagFrameFinish,
         10, 19, tail, sizeof(tail));
    assert(submittedCount == 0);

    // Head arrives late and is spliced in ahead of the tail.
    Feed(1, 1000, 1, VideoFrameFlagSubFrameAdvance, 0, 9, head, sizeof(head));

    assert(submittedCount == 1);
    assert(submitted[0].len == sizeof(head) + sizeof(tail));
    assert(memcmp(submitted[0].data, head, sizeof(head)) == 0);
    assert(memcmp(submitted[0].data + sizeof(head), tail, sizeof(tail)) == 0);
    assert(TakeKeyframeRequests() == 0);
}

/**
 * A gap in the sequence numbers means a lost frame: request a keyframe, and drop everything until
 * one arrives rather than feeding the decoder a hole.
 */
/**
 * Every arrival order of a four-fragment frame must assemble to the same bytes. AddPartialFrame
 * takes a fast path when the new fragment belongs to the same frame as the list tail and sorts
 * after it, skipping the list walk; this pins that the shortcut agrees with the walk for all 24
 * permutations rather than just the in-order one.
 */
static void test_fragment_permutations_assemble_identically(void) {
    static const uint8_t payloads[4][2] = {{0x11, 0x12},
                                           {0x21, 0x22},
                                           {0x31, 0x32},
                                           {0x41, 0x42}};
    uint8_t expected[8];
    for (int i = 0; i < 4; i++) {
        memcpy(&expected[i * 2], payloads[i], 2);
    }

    int order[4] = {0, 1, 2, 3};
    for (int a = 0; a < 4; a++) {
        for (int b = 0; b < 4; b++) {
            if (b == a) continue;
            for (int c = 0; c < 4; c++) {
                if (c == a || c == b) continue;
                int d = 6 - a - b - c;
                order[0] = a;
                order[1] = b;
                order[2] = c;
                order[3] = d;

                Reset();
                for (int i = 0; i < 4; i++) {
                    int fragment = order[i];
                    uint8_t flags = VideoFrameFlagSubFrameAdvance;
                    // The first packet to arrive carries the keyframe flag, so the channel is not
                    // sitting in waitingKeyFrame; the last fragment of the frame closes it.
                    if (i == 0) flags |= VideoFrameFlagKeyFrame;
                    if (fragment == 3) flags |= VideoFrameFlagFrameFinish;
                    Feed(7, 1000, (uint16_t) i, flags, (uint16_t) (fragment * 10),
                         (uint16_t) (fragment * 10 + 9), payloads[fragment], 2);
                }

                assert(submittedCount == 1);
                assert(submitted[0].len == sizeof(expected));
                assert(memcmp(submitted[0].data, expected, sizeof(expected)) == 0);
                assert(TakeKeyframeRequests() == 0);
            }
        }
    }
}

static void test_packet_loss_requests_keyframe(void) {
    Reset();
    const uint8_t payload[] = {0x55, 0x66};
    const uint8_t after[] = {0x77, 0x88};

    Feed(1, 1000, 0, VideoFrameFlagKeyFrame | VideoFrameFlagSubFrameAdvance | VideoFrameFlagFrameFinish,
         0, 3, payload, sizeof(payload));
    assert(submittedCount == 1);
    assert(TakeKeyframeRequests() == 0);

    // Sequence 1 never arrives; 2 shows up instead.
    Feed(2, 2000, 2, VideoFrameFlagSubFrameAdvance | VideoFrameFlagFrameFinish, 0, 3, after, sizeof(after));
    assert(TakeKeyframeRequests() == 1);
    assert(submittedCount == 1 && "frame after a loss must be dropped, not decoded");

    // Still waiting: further non-keyframe data stays dropped.
    Feed(3, 3000, 3, VideoFrameFlagSubFrameAdvance | VideoFrameFlagFrameFinish, 0, 3, after, sizeof(after));
    assert(submittedCount == 1);

    // The keyframe clears the wait and streaming resumes.
    const uint8_t recovered[] = {0x99};
    Feed(4, 4000, 4, VideoFrameFlagKeyFrame | VideoFrameFlagSubFrameAdvance | VideoFrameFlagFrameFinish,
         0, 3, recovered, sizeof(recovered));
    assert(submittedCount == 2);
    assert(submitted[1].len == sizeof(recovered));
    assert(submitted[1].data[0] == 0x99);
}

/**
 * Jitter: a fragment goes missing and later fragments pile up behind it. Once the span between the
 * oldest and newest pending fragment passes 150 ms the channel gives up and asks for a keyframe,
 * rather than stalling until the next natural one. Mirrors CheckOverflow.
 */
static void test_jitter_stall_requests_keyframe(void) {
    Reset();
    const uint8_t frag[] = {0xC0, 0xDE};

    // Establish sync with a keyframe so the sequence check stays quiet.
    Feed(1, 1000, 0, VideoFrameFlagKeyFrame | VideoFrameFlagSubFrameAdvance | VideoFrameFlagFrameFinish,
         0, 3, frag, sizeof(frag));
    assert(submittedCount == 1);
    Reset();

    // A tail fragment with no head: cannot assemble, stays pending.
    Feed(2, 10000, 1, VideoFrameFlagSubFrameAdvance, 10, 19, frag, sizeof(frag));
    assert(submittedCount == 0);
    assert(TakeKeyframeRequests() == 0 && "one pending fragment is not yet a stall");

    // A second pending fragment, still inside the window.
    Feed(3, 10000 + OVERFLOW_SPAN / 2, 2, VideoFrameFlagSubFrameAdvance, 30, 39, frag, sizeof(frag));
    assert(TakeKeyframeRequests() == 0 && "under 150 ms is not a stall");

    // Now the span from oldest to newest exceeds 150 ms.
    Feed(4, 10000 + OVERFLOW_SPAN + 1, 3, VideoFrameFlagSubFrameAdvance, 50, 59, frag, sizeof(frag));
    assert(TakeKeyframeRequests() == 1);
    assert(submittedCount == 0);
}

/**
 * Regression for the defensive reset in the submit block: a frame whose last fragment carries
 * FrameFinish without SubFrameAdvance leaves expectedSubFrameStart non-zero, which used to stall
 * the following frame until a keyframe recovered it.
 */
static void test_frame_finished_without_advance_does_not_stall_next(void) {
    Reset();
    const uint8_t a[] = {0x01};
    const uint8_t b[] = {0x02};
    const uint8_t next[] = {0x03};

    Feed(1, 1000, 0, VideoFrameFlagKeyFrame | VideoFrameFlagSubFrameAdvance, 0, 9, a, sizeof(a));
    // FrameFinish, but no SubFrameAdvance: AssembleFrame will not reset the counter itself.
    Feed(1, 1000, 1, VideoFrameFlagFrameFinish, 10, 19, b, sizeof(b));
    assert(submittedCount == 1);
    assert(submitted[0].len == 2);

    // The next frame starts at 0 again. Without the reset this mismatches and never assembles.
    Feed(2, 2000, 2, VideoFrameFlagSubFrameAdvance | VideoFrameFlagFrameFinish, 0, 9, next, sizeof(next));
    assert(submittedCount == 2);
    assert(submitted[1].len == 1);
    assert(submitted[1].data[0] == 0x03);
    assert(TakeKeyframeRequests() == 0);
}

/** The encrypted path decrypts in place and hands the plaintext on unchanged. */
static void test_encrypted_frame(void) {
    Reset();
    const uint8_t plain[] = {0xDE, 0xAD, 0xBE, 0xEF, 0x01, 0x02, 0x03, 0x04,
                             0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C,
                             0x0D, 0x0E, 0x0F, 0x10};
    uint8_t cipher[128];
    size_t cipherLen = sizeof(cipher);
    assert(IHS_CryptoSymmetricEncryptWithIV(plain, sizeof(plain), EmptyIV, sizeof(EmptyIV),
                                            session->info.sessionKey, session->info.sessionKeyLen,
                                            false, cipher, &cipherLen) == 0);
    // More than one block, so an in-place decrypt that clobbered its own IV chain would show up.
    assert(cipherLen > 16);

    Feed(1, 1000, 0,
         VideoFrameFlagKeyFrame | VideoFrameFlagEncrypted | VideoFrameFlagSubFrameAdvance | VideoFrameFlagFrameFinish,
         0, 3, cipher, cipherLen);

    assert(submittedCount == 1);
    assert(submitted[0].len == sizeof(plain));
    assert(memcmp(submitted[0].data, plain, sizeof(plain)) == 0);
    assert(TakeKeyframeRequests() == 0);
}

/** A corrupt ciphertext must be reported as a loss rather than passed to the decoder. */
static void test_undecryptable_frame_requests_keyframe(void) {
    Reset();
    uint8_t garbage[32];
    memset(garbage, 0xFF, sizeof(garbage));

    Feed(1, 1000, 0,
         VideoFrameFlagKeyFrame | VideoFrameFlagEncrypted | VideoFrameFlagSubFrameAdvance | VideoFrameFlagFrameFinish,
         0, 3, garbage, sizeof(garbage));

    assert(submittedCount == 0);
    assert(TakeKeyframeRequests() == 1);
}

int main(void) {
    SetUp();
    test_single_frame();
    test_out_of_order_fragments();
    test_fragment_permutations_assemble_identically();
    test_packet_loss_requests_keyframe();
    test_jitter_stall_requests_keyframe();
    test_frame_finished_without_advance_does_not_stall_next();
    test_encrypted_frame();
    test_undecryptable_frame_requests_keyframe();
    TearDown();
    printf("video delivery tests OK\n");
    return 0;
}
