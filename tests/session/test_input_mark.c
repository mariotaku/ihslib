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
 * Two things are checked here: the input-mark ring on its own, and the shape of what the mouse
 * motion senders actually put on the wire. The second half is the point of the API change — the
 * relative form must leave x_normalized/y_normalized *absent*, not zeroed, so it is verified by
 * decoding the queued packet rather than by reading the source.
 */

#include <assert.h>
#include <string.h>
#include <stdio.h>

#include "session/session_pri.h"
#include "session/input_mark.h"
#include "session/frame.h"
#include "session/channels/ch_control.h"
#include "ihs_queue.h"

#include "test_session.h"

/** Mirrors the definition in session.c; IHS_QueueItem is deliberately owner-defined. */
typedef struct IHS_QueueItem {
    IHS_SessionPacket packet;
    bool retransmit;
} QueuedPacket;

static uint64_t expectedSequence = 0;

/**
 * Unwrap one queued control packet as a CInputMouseMotionMsg. Body is
 * [control message type][encrypted payload].
 */
static CInputMouseMotionMsg *TakeMouseMotion(IHS_Session *session) {
    IHS_QueueItem *item = IHS_QueuePoll(session->sendQueue);
    assert(item != NULL);
    QueuedPacket *queued = (QueuedPacket *) item;

    IHS_Buffer *body = &queued->packet.body;
    assert(body->size > 1);
    assert(IHS_BufferPointerAt(body, 0)[0] == k_EStreamControlInputMouseMotion);

    IHS_Buffer cipher = *body;
    IHS_BufferOffsetBy(&cipher, 1);

    IHS_Buffer plain = IHS_BUFFER_INIT(256, 8192);
    uint64_t actualSequence = 0;
    IHS_SessionFrameDecryptResult decrypted = IHS_SessionFrameDecrypt(session, &cipher, &plain, expectedSequence,
                                                                     &actualSequence);
    assert(decrypted == IHS_SessionFrameDecryptOK);
    expectedSequence++;

    CInputMouseMotionMsg *message = cinput_mouse_motion_msg__unpack(NULL, plain.size, IHS_BufferPointer(&plain));
    assert(message != NULL);

    IHS_BufferClear(&plain, true);
    IHS_SessionPacketClear(&queued->packet, true);
    IHS_QueueItemFree(item);
    return message;
}

static void TestRing(void) {
    IHS_SessionInputMarks marks;
    IHS_SessionInputMarksInit(&marks);

    IHS_SessionInputLatency latency;
    // Nothing measured until a frame echoes a mark back.
    assert(!IHS_SessionInputMarksGetLatest(&marks, &latency));

    // Marks start at 1 and count up.
    assert(IHS_SessionInputMarkNext(&marks, 100) == 1);
    assert(IHS_SessionInputMarkNext(&marks, 200) == 2);

    // A frame carrying mark 0 reflects no input and must not resolve to anything.
    assert(!IHS_SessionInputMarkFinish(&marks, 0, 0, 500));
    assert(!IHS_SessionInputMarksGetLatest(&marks, &latency));

    // Mark 1 was created with event time 100. Round trip is measured from that, not from the send.
    assert(IHS_SessionInputMarkFinish(&marks, 1, 7777, 900));
    assert(IHS_SessionInputMarksGetLatest(&marks, &latency));
    assert(latency.inputMark == 1);
    assert(latency.roundTrip == 900 - 100);
    assert(latency.hostRecvTimestamp == 7777);

    // A mark that was never issued must not match a stale or zeroed slot.
    assert(!IHS_SessionInputMarkFinish(&marks, 3, 0, 1000));

    // Filling the ring evicts the oldest marks: the slot for mark 1 is reused by mark 1025.
    for (int i = 0; i < IHS_INPUT_MARK_RING_SIZE; i++) {
        IHS_SessionInputMarkNext(&marks, 0);
    }
    assert(!IHS_SessionInputMarkFinish(&marks, 1, 0, 2000));
    // ...and the mark that replaced it still resolves.
    assert(IHS_SessionInputMarkFinish(&marks, 1 + IHS_INPUT_MARK_RING_SIZE, 0, 2000));

    IHS_SessionInputMarksDeinit(&marks);
}

static void TestCounterWrapSkipsZero(void) {
    IHS_SessionInputMarks marks;
    IHS_SessionInputMarksInit(&marks);
    // Walk the counter all the way around. 0 is the host's "no mark" sentinel, so the counter must
    // step straight from 0xFFFF to 1 — CreateInputMark @ 0x1fb264 loops until it is non-zero.
    uint16_t previous = 0;
    bool sawWrap = false;
    for (uint32_t i = 0; i < 0x10000u + 4u; i++) {
        uint16_t mark = IHS_SessionInputMarkNext(&marks, 0);
        assert(mark != 0);
        if (previous == 0xFFFF) {
            assert(mark == 1);
            sawWrap = true;
        }
        previous = mark;
    }
    assert(sawWrap);
    IHS_SessionInputMarksDeinit(&marks);
}

static void TestWireShape(void) {
    IHS_Session *session = IHS_TestSessionCreate();
    session->state.streamingInput = true;

    // The absolute form carries all four fields, as CStreamClient::SendMouseMotion @ 0x1f910c does.
    assert(IHS_SessionSendMouseMotion(session, 4242, 0.25f, 0.75f, -3, 7));
    CInputMouseMotionMsg *message = TakeMouseMotion(session);
    assert(message->has_input_mark && message->input_mark == 1);
    assert(message->has_x_normalized && message->x_normalized == 0.25f);
    assert(message->has_y_normalized && message->y_normalized == 0.75f);
    assert(message->has_dx && message->dx == -3);
    assert(message->has_dy && message->dy == 7);
    cinput_mouse_motion_msg__free_unpacked(message, NULL);

    // The relative form must OMIT the position fields rather than send zeros: overload @ 0x1f92f0
    // never touches them, so the host can tell "position unknown" from "pointer at top-left".
    assert(IHS_SessionSendMouseMotionRelative(session, 4243, 5, -6));
    message = TakeMouseMotion(session);
    assert(message->has_input_mark && message->input_mark == 2);
    assert(!message->has_x_normalized);
    assert(!message->has_y_normalized);
    assert(message->has_dx && message->dx == 5);
    assert(message->has_dy && message->dy == -6);
    cinput_mouse_motion_msg__free_unpacked(message, NULL);

    // Every input sender draws from the same counter, so a keystroke advances it too.
    assert(IHS_SessionSendKeyDown(session, 0x1a));
    IHS_QueueItem *item = IHS_QueuePoll(session->sendQueue);
    assert(item != NULL);
    IHS_SessionPacketClear(&((QueuedPacket *) item)->packet, true);
    IHS_QueueItemFree(item);
    expectedSequence++;

    assert(IHS_SessionSendMouseMotionRelative(session, 4244, 1, 1));
    message = TakeMouseMotion(session);
    assert(message->input_mark == 4);
    cinput_mouse_motion_msg__free_unpacked(message, NULL);

    // The streamingInput gate runs before the mark is taken, so a send the server has disabled
    // neither queues a packet nor burns a mark.
    session->state.streamingInput = false;
    assert(!IHS_SessionSendMouseMotionRelative(session, 4245, 1, 1));
    assert(IHS_QueueIsEmpty(session->sendQueue));
    session->state.streamingInput = true;
    assert(IHS_SessionSendMouseMotionRelative(session, 4246, 1, 1));
    message = TakeMouseMotion(session);
    assert(message->input_mark == 5);
    cinput_mouse_motion_msg__free_unpacked(message, NULL);

    // The public clock helper must actually be callable and moving.
    uint32_t t0 = IHS_InputTimestampNow();
    assert(t0 != 0);

    IHS_SessionDestroy(session);
}

int main(void) {
    IHS_Init();
    TestRing();
    TestCounterWrapSkipsZero();
    TestWireShape();
    IHS_Quit();
    printf("input_mark tests OK\n");
    return 0;
}
