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
 * A retransmission is created asynchronously: RetransmissionTimerRun hands the packet to the send
 * queue and RetransmissionTimerEnd frees the pending entry, both on the receive thread, but the new
 * entry does not exist until SessionSendWorker calls IHS_RetransmissionQueue on the send thread.
 *
 * An ACK arriving in that window cancels nothing, and the twin — born afterwards — retransmits to
 * the attempt limit against a packet the host already has. These cases drive the two halves of that
 * handoff directly, in the order the threads would produce.
 */

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "session/session_pri.h"
#include "session/retransmission.h"

#include "test_session.h"

#define CHANNEL_ID 3
#define PACKET_ID 1609
#define FRAGMENT_ID 0

static IHS_Session *session = NULL;

/** A packet shaped the way IHS_RetransmissionQueue expects: body owned, header offset applied. */
static void MakePacket(IHS_SessionPacket *packet, uint16_t packetId, uint8_t retransmitCount) {
    memset(packet, 0, sizeof(*packet));
    packet->header.type = IHS_SessionPacketTypeReliable;
    packet->header.channelId = CHANNEL_ID;
    packet->header.packetId = packetId;
    packet->header.fragmentId = FRAGMENT_ID;
    packet->header.retransmitCount = retransmitCount;

    // The queue asserts an owned body sitting past the header, which is the shape
    // IHS_SessionChannelInitializePacketHeader leaves behind.
    static const uint8_t payload[] = {'t', 'w', 'i', 'n'};
    IHS_BufferInit(&packet->body, IHS_PACKET_HEADER_SIZE + sizeof(payload), 256);
    IHS_BufferAppendMem(&packet->body, (const uint8_t *) "\0\0\0\0\0\0\0\0\0\0\0\0\0", IHS_PACKET_HEADER_SIZE);
    IHS_BufferAppendMem(&packet->body, payload, sizeof(payload));
    IHS_BufferOffsetBy(&packet->body, IHS_PACKET_HEADER_SIZE);
}

/** Drain whatever the timer left behind, so each case starts clean. */
static void ClearPending(IHS_SessionRetransmission *retransmission, uint16_t packetId) {
    IHS_RetransmissionCancel(retransmission, CHANNEL_ID, packetId, FRAGMENT_ID);
}

/**
 * The handoff: an ACK lands while the retransmission is between the timer and the send worker, so
 * the cancel finds nothing. The twin that arrives afterwards must be dropped.
 */
static void test_twin_after_cancel_is_dropped(void) {
    IHS_SessionRetransmission retransmission;
    IHS_RetransmissionInit(&retransmission, session);

    // The ACK arrives with nothing queued under the identity — the handoff window.
    assert(IHS_RetransmissionCancel(&retransmission, CHANNEL_ID, PACKET_ID, FRAGMENT_ID) == false);

    // Now the send worker gets round to creating the twin.
    IHS_SessionPacket twin;
    MakePacket(&twin, PACKET_ID, 1);
    assert(IHS_RetransmissionQueue(&retransmission, &twin) == false &&
           "a twin whose ACK already landed must not be queued again");
    IHS_SessionPacketClear(&twin, true);

    IHS_RetransmissionDeinit(&retransmission);
}

/** One cancel suppresses exactly one twin; a later retransmission is legitimate again. */
static void test_entry_is_consumed_once(void) {
    IHS_SessionRetransmission retransmission;
    IHS_RetransmissionInit(&retransmission, session);

    assert(IHS_RetransmissionCancel(&retransmission, CHANNEL_ID, PACKET_ID, FRAGMENT_ID) == false);

    IHS_SessionPacket first;
    MakePacket(&first, PACKET_ID, 1);
    assert(IHS_RetransmissionQueue(&retransmission, &first) == false);
    IHS_SessionPacketClear(&first, true);

    IHS_SessionPacket second;
    MakePacket(&second, PACKET_ID, 1);
    assert(IHS_RetransmissionQueue(&retransmission, &second) == true &&
           "the cancelled entry is consumed by the first twin, not standing forever");
    ClearPending(&retransmission, PACKET_ID);

    IHS_RetransmissionDeinit(&retransmission);
}

/**
 * A first send carries retransmitCount 0 and cannot be a twin — its identity has had no chance to
 * be cancelled. Suppressing one would silently drop a packet that was never sent.
 */
static void test_first_send_is_never_dropped(void) {
    IHS_SessionRetransmission retransmission;
    IHS_RetransmissionInit(&retransmission, session);

    assert(IHS_RetransmissionCancel(&retransmission, CHANNEL_ID, PACKET_ID, FRAGMENT_ID) == false);

    IHS_SessionPacket fresh;
    MakePacket(&fresh, PACKET_ID, 0);
    assert(IHS_RetransmissionQueue(&retransmission, &fresh) == true &&
           "a first send must never be suppressed by a stale cancelled identity");
    ClearPending(&retransmission, PACKET_ID);

    IHS_RetransmissionDeinit(&retransmission);
}

/** A cancel that actually found something records nothing: there is no twin in flight to suppress. */
static void test_successful_cancel_does_not_suppress(void) {
    IHS_SessionRetransmission retransmission;
    IHS_RetransmissionInit(&retransmission, session);

    IHS_SessionPacket queued;
    MakePacket(&queued, PACKET_ID, 1);
    assert(IHS_RetransmissionQueue(&retransmission, &queued) == true);
    assert(IHS_RetransmissionCancel(&retransmission, CHANNEL_ID, PACKET_ID, FRAGMENT_ID) == true);

    // The identity was never recorded, so a genuinely new retransmission still goes through.
    IHS_SessionPacket later;
    MakePacket(&later, PACKET_ID, 1);
    assert(IHS_RetransmissionQueue(&retransmission, &later) == true);
    ClearPending(&retransmission, PACKET_ID);

    IHS_RetransmissionDeinit(&retransmission);
}

/** Identities are matched whole — a different packet id is not suppressed by a neighbour's cancel. */
static void test_other_identities_unaffected(void) {
    IHS_SessionRetransmission retransmission;
    IHS_RetransmissionInit(&retransmission, session);

    assert(IHS_RetransmissionCancel(&retransmission, CHANNEL_ID, PACKET_ID, FRAGMENT_ID) == false);

    IHS_SessionPacket other;
    MakePacket(&other, PACKET_ID + 1, 1);
    assert(IHS_RetransmissionQueue(&retransmission, &other) == true);
    ClearPending(&retransmission, PACKET_ID + 1);

    IHS_RetransmissionDeinit(&retransmission);
}

int main(void) {
    IHS_Init();
    session = IHS_TestSessionCreate();
    assert(session != NULL);

    test_twin_after_cancel_is_dropped();
    test_entry_is_consumed_once();
    test_first_send_is_never_dropped();
    test_successful_cancel_does_not_suppress();
    test_other_identities_unaffected();

    IHS_SessionDestroy(session);
    IHS_Quit();
    printf("retransmission twin tests OK\n");
    return 0;
}
