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
 * Mouse motion coalescing: many queued events collapse into one message, and a click never overtakes
 * the motion that positioned the pointer. Messages are taken back off the session's send queue and
 * decrypted, so what is checked is what would have gone on the wire.
 */

#include <assert.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "session/session_pri.h"
#include "session/frame.h"
#include "session/channels/ch_control.h"
#include "ihs_queue.h"
#include "ihslib/input.h"

#include "test_session.h"

/** Mirrors the definition in session.c — see the note in test_control_hid.c. */
typedef struct IHS_QueueItem {
    IHS_SessionPacket packet;
    bool retransmit;
} QueuedPacket;

static uint64_t expectedSequence = 0;

/**
 * Take the next queued control message: [control message type][encrypted payload]. The plaintext is
 * handed back for the caller to unpack.
 */
static EStreamControlMessage TakeControlMessage(IHS_Session *session, IHS_Buffer *plain) {
    IHS_QueueItem *item = IHS_QueuePoll(session->sendQueue);
    assert(item != NULL);
    QueuedPacket *queued = (QueuedPacket *) item;

    IHS_Buffer *body = &queued->packet.body;
    assert(body->size > 1);
    EStreamControlMessage type = IHS_BufferPointerAt(body, 0)[0];

    IHS_Buffer cipher = *body;
    IHS_BufferOffsetBy(&cipher, 1);

    uint64_t actualSequence = 0;
    IHS_SessionFrameDecryptResult decrypted = IHS_SessionFrameDecrypt(session, &cipher, plain, expectedSequence,
                                                                      &actualSequence);
    assert(decrypted == IHS_SessionFrameDecryptOK);
    expectedSequence++;

    IHS_SessionPacketClear(&queued->packet, true);
    IHS_QueueItemFree(item);
    return type;
}

static CInputMouseMotionMsg *TakeMotion(IHS_Session *session) {
    IHS_Buffer plain = IHS_BUFFER_INIT(1024, 8192);
    EStreamControlMessage type = TakeControlMessage(session, &plain);
    assert(type == k_EStreamControlInputMouseMotion);
    CInputMouseMotionMsg *message = cinput_mouse_motion_msg__unpack(NULL, plain.size, IHS_BufferPointer(&plain));
    assert(message != NULL);
    IHS_BufferClear(&plain, true);
    return message;
}

static void TestQueuedMotionCollapsesIntoOne(IHS_Session *session) {
    // Twenty events from a high-DPI mouse in one pass of the application's event loop.
    for (int i = 0; i < 20; i++) {
        IHS_SessionQueueMouseMotion(session, 0.1f * (float) i, 0.2f * (float) i, 2, -1);
    }
    // Nothing is on the wire until the flush.
    assert(IHS_QueueIsEmpty(session->sendQueue));

    assert(IHS_SessionFlushMouseMotion(session));
    CInputMouseMotionMsg *message = TakeMotion(session);
    // Deltas summed, position taken from the last event.
    assert(message->has_dx && message->dx == 40);
    assert(message->has_dy && message->dy == -20);
    assert(message->has_x_normalized && fabsf(message->x_normalized - 1.9f) < 0.0001f);
    assert(message->has_y_normalized && fabsf(message->y_normalized - 3.8f) < 0.0001f);
    // One mark for the whole batch, not twenty.
    assert(message->has_input_mark && message->input_mark != 0);
    cinput_mouse_motion_msg__free_unpacked(message, NULL);
    assert(IHS_QueueIsEmpty(session->sendQueue));

    // A flush with nothing pending is a no-op, not an empty message.
    assert(!IHS_SessionFlushMouseMotion(session));
    assert(IHS_QueueIsEmpty(session->sendQueue));
}

static void TestDeltasClearButPositionSticks(IHS_Session *session) {
    IHS_SessionQueueMouseMotion(session, 0.5f, 0.25f, 3, 4);
    assert(IHS_SessionFlushMouseMotion(session));
    cinput_mouse_motion_msg__free_unpacked(TakeMotion(session), NULL);

    // ClearQueuedMouseMotion @ 0x22d09c leaves x/y behind, so a relative-only queue afterwards
    // still reports where the pointer is, and the deltas start again from zero.
    IHS_SessionQueueMouseMotionRelative(session, 1, 1);
    assert(IHS_SessionFlushMouseMotion(session));
    CInputMouseMotionMsg *message = TakeMotion(session);
    assert(message->dx == 1 && message->dy == 1);
    assert(message->has_x_normalized && fabsf(message->x_normalized - 0.5f) < 0.0001f);
    assert(message->has_y_normalized && fabsf(message->y_normalized - 0.25f) < 0.0001f);
    cinput_mouse_motion_msg__free_unpacked(message, NULL);
}

/** Runs first: the queued position is sticky for the life of the session, by design. */
static void TestRelativeOnlyOmitsPosition(IHS_Session *session) {
    // A session that has only ever seen relative motion must not claim the pointer is at the
    // top-left corner — the reference leaves the fields absent instead (0x1f92f0).
    IHS_SessionQueueMouseMotionRelative(session, -5, 7);
    IHS_SessionQueueMouseMotionRelative(session, 2, 3);
    assert(IHS_SessionFlushMouseMotion(session));
    CInputMouseMotionMsg *message = TakeMotion(session);
    assert(message->dx == -3 && message->dy == 10);
    assert(!message->has_x_normalized);
    assert(!message->has_y_normalized);
    cinput_mouse_motion_msg__free_unpacked(message, NULL);
}

static void TestClickFlushesFirst(IHS_Session *session) {
    IHS_SessionQueueMouseMotion(session, 0.75f, 0.75f, 8, 9);
    // BHandleEvent flushes immediately before the button send (0x21a5a8), so the motion has to come
    // off the queue first.
    assert(IHS_SessionSendMouseDown(session, IHS_MOUSE_BUTTON_LEFT));

    CInputMouseMotionMsg *motion = TakeMotion(session);
    assert(motion->dx == 8 && motion->dy == 9);
    cinput_mouse_motion_msg__free_unpacked(motion, NULL);

    IHS_Buffer plain = IHS_BUFFER_INIT(1024, 8192);
    assert(TakeControlMessage(session, &plain) == k_EStreamControlInputMouseDown);
    IHS_BufferClear(&plain, true);
    assert(IHS_QueueIsEmpty(session->sendQueue));

    // The wheel and button-up senders flush too.
    IHS_SessionQueueMouseMotionRelative(session, 1, 0);
    assert(IHS_SessionSendMouseWheel(session, IHS_MOUSE_WHEEL_UP));
    cinput_mouse_motion_msg__free_unpacked(TakeMotion(session), NULL);
    plain = (IHS_Buffer) IHS_BUFFER_INIT(1024, 8192);
    assert(TakeControlMessage(session, &plain) == k_EStreamControlInputMouseWheel);
    IHS_BufferClear(&plain, true);

    IHS_SessionQueueMouseMotionRelative(session, 0, 1);
    assert(IHS_SessionSendMouseUp(session, IHS_MOUSE_BUTTON_LEFT));
    cinput_mouse_motion_msg__free_unpacked(TakeMotion(session), NULL);
    plain = (IHS_Buffer) IHS_BUFFER_INIT(1024, 8192);
    assert(TakeControlMessage(session, &plain) == k_EStreamControlInputMouseUp);
    IHS_BufferClear(&plain, true);
    assert(IHS_QueueIsEmpty(session->sendQueue));
}

int main() {
    IHS_Init();
    IHS_Session *session = IHS_TestSessionCreate();
    assert(session != NULL);
    assert(session->channels[IHS_SessionChannelIdControl] != NULL);

    TestRelativeOnlyOmitsPosition(session);
    TestQueuedMotionCollapsesIntoOne(session);
    TestDeltasClearButPositionSticks(session);
    TestClickFlushesFirst(session);

    IHS_SessionDestroy(session);
    IHS_Quit();
    return 0;
}
