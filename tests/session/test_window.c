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
 * The reassembly window's handling of fragment counts. `fragmentId` is signed and arrives straight
 * off the wire with no validation, so these cases feed it the values a corrupt or hostile datagram
 * could carry and check that the window neither spins nor walks off its own array.
 */

#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "session/window.h"
#include "session/packet.h"
#include "session/session_pri.h"
#include "session/channels/channel.h"
#include "ihs_queue.h"

#include "test_session.h"

/** Mirrors the definition in session.c; IHS_QueueItem is deliberately owner-defined. */
typedef struct IHS_QueueItem {
    IHS_SessionPacket packet;
    bool retransmit;
} QueuedPacket;

static void PacketInit(IHS_SessionPacket *packet, IHS_SessionPacketType type, int16_t fragmentId,
                       uint16_t packetId, const char *payload) {
    memset(packet, 0, sizeof(*packet));
    packet->header.type = type;
    packet->header.fragmentId = fragmentId;
    packet->header.packetId = packetId;
    IHS_BufferInit(&packet->body, 32, 32);
    IHS_BufferAppendMem(&packet->body, (const uint8_t *) payload, strlen(payload));
}

static void AddPacket(IHS_SessionPacketsWindow *window, IHS_SessionPacketType type, int16_t fragmentId,
                      uint16_t packetId, const char *payload) {
    IHS_SessionPacket packet;
    PacketInit(&packet, type, fragmentId, packetId, payload);
    assert(IHS_SessionPacketsWindowAdd(window, &packet));
    IHS_SessionPacketClear(&packet, true);
}

/** A single-packet frame carries fragmentId 0, and comes straight back out. */
static void TestSingleFragment(void) {
    IHS_SessionPacketsWindow *window = IHS_SessionPacketsWindowCreate(64);
    AddPacket(window, IHS_SessionPacketTypeUnreliable, 0, 0, "hello");

    IHS_SessionFrame frame;
    memset(&frame, 0, sizeof(frame));
    IHS_BufferInit(&frame.body, 64, 4096);

    assert(IHS_SessionPacketsWindowPoll(window, &frame));
    assert(frame.body.size == 5);
    assert(memcmp(IHS_BufferPointer(&frame.body), "hello", 5) == 0);
    IHS_SessionPacketsWindowReleaseFrame(&frame);
    assert(IHS_SessionPacketsWindowSize(window) == 0);
    assert(!IHS_SessionPacketsWindowPoll(window, &frame));

    IHS_BufferClear(&frame.body, true);
    IHS_SessionPacketsWindowDestroy(window);
}

/** Two fragments: the head announces one more, and the pair is joined in order. */
static void TestTwoFragments(void) {
    IHS_SessionPacketsWindow *window = IHS_SessionPacketsWindowCreate(64);
    AddPacket(window, IHS_SessionPacketTypeUnreliable, 1, 0, "abc");
    AddPacket(window, IHS_SessionPacketTypeUnreliableFrag, 0, 1, "def");

    IHS_SessionFrame frame;
    memset(&frame, 0, sizeof(frame));
    IHS_BufferInit(&frame.body, 64, 4096);

    assert(IHS_SessionPacketsWindowPoll(window, &frame));
    assert(frame.body.size == 6);
    assert(memcmp(IHS_BufferPointer(&frame.body), "abcdef", 6) == 0);
    IHS_SessionPacketsWindowReleaseFrame(&frame);

    IHS_BufferClear(&frame.body, true);
    IHS_SessionPacketsWindowDestroy(window);
}

/** An incomplete frame waits rather than delivering a truncated one. */
static void TestIncompleteWaits(void) {
    IHS_SessionPacketsWindow *window = IHS_SessionPacketsWindowCreate(64);
    AddPacket(window, IHS_SessionPacketTypeUnreliable, 1, 0, "abc");

    IHS_SessionFrame frame;
    memset(&frame, 0, sizeof(frame));
    IHS_BufferInit(&frame.body, 64, 4096);

    // Poll must keep saying "not yet" without consuming the head.
    for (int i = 0; i < 4; i++) {
        assert(!IHS_SessionPacketsWindowPoll(window, &frame));
        assert(IHS_SessionPacketsWindowSize(window) == 1);
    }

    IHS_BufferClear(&frame.body, true);
    IHS_SessionPacketsWindowDestroy(window);
}

/**
 * fragmentId == -1 makes the fragment count 0. Trusted, that reports a frame while leaving head.pos
 * untouched, so IHS_SessionChannelData's worker loop spins on it forever at full CPU. The head must
 * be consumed instead.
 */
static void TestZeroCountDoesNotSpin(void) {
    IHS_SessionPacketsWindow *window = IHS_SessionPacketsWindowCreate(64);
    AddPacket(window, IHS_SessionPacketTypeUnreliable, -1, 0, "junk");

    IHS_SessionFrame frame;
    memset(&frame, 0, sizeof(frame));
    IHS_BufferInit(&frame.body, 64, 4096);

    assert(!IHS_SessionPacketsWindowPoll(window, &frame));
    // Consumed, not left in place: a second Poll would otherwise see the same head again.
    assert(IHS_SessionPacketsWindowSize(window) == 0);
    assert(!IHS_SessionPacketsWindowPoll(window, &frame));

    IHS_BufferClear(&frame.body, true);
    IHS_SessionPacketsWindowDestroy(window);
}

/**
 * A fragment count below zero used to drive head.pos negative — an assert in debug and an
 * out-of-bounds index in release, with IHS_SessionPacketsWindowSize reporting nonsense afterwards.
 */
static void TestNegativeCountDoesNotCorrupt(void) {
    for (int16_t fragmentId = -2; fragmentId >= -600; fragmentId -= 71) {
        IHS_SessionPacketsWindow *window = IHS_SessionPacketsWindowCreate(64);
        AddPacket(window, IHS_SessionPacketTypeUnreliable, fragmentId, 0, "junk");

        IHS_SessionFrame frame;
        memset(&frame, 0, sizeof(frame));
        IHS_BufferInit(&frame.body, 64, 4096);

        assert(!IHS_SessionPacketsWindowPoll(window, &frame));
        assert(IHS_SessionPacketsWindowSize(window) == 0);
        assert(IHS_SessionPacketsWindowAvailable(window) == 64);

        // The window must still be usable: a well-formed frame after the junk still comes through.
        AddPacket(window, IHS_SessionPacketTypeUnreliable, 0, 1, "good");
        assert(IHS_SessionPacketsWindowPoll(window, &frame));
        assert(frame.body.size == 4);
        assert(memcmp(IHS_BufferPointer(&frame.body), "good", 4) == 0);
        IHS_SessionPacketsWindowReleaseFrame(&frame);

        IHS_BufferClear(&frame.body, true);
        IHS_SessionPacketsWindowDestroy(window);
    }
}

/** A run of malformed heads is skipped in one Poll, and the good frame behind them survives. */
static void TestSkipsRunOfMalformedHeads(void) {
    IHS_SessionPacketsWindow *window = IHS_SessionPacketsWindowCreate(64);
    AddPacket(window, IHS_SessionPacketTypeUnreliable, -1, 0, "junk0");
    AddPacket(window, IHS_SessionPacketTypeUnreliable, -3, 1, "junk1");
    AddPacket(window, IHS_SessionPacketTypeUnreliable, -1, 2, "junk2");
    AddPacket(window, IHS_SessionPacketTypeUnreliable, 0, 3, "good");

    IHS_SessionFrame frame;
    memset(&frame, 0, sizeof(frame));
    IHS_BufferInit(&frame.body, 64, 4096);

    assert(IHS_SessionPacketsWindowPoll(window, &frame));
    assert(frame.body.size == 4);
    assert(memcmp(IHS_BufferPointer(&frame.body), "good", 4) == 0);
    IHS_SessionPacketsWindowReleaseFrame(&frame);
    assert(IHS_SessionPacketsWindowSize(window) == 0);

    IHS_BufferClear(&frame.body, true);
    IHS_SessionPacketsWindowDestroy(window);
}

/**
 * Fragment a frame with the real send path, then reassemble it with the real receive path. The two
 * disagreed on what the head packet's fragmentId means: the sender advertised the total (plus one),
 * the receiver reads it as "packets that follow". Anything but a round trip lets that drift back in.
 */
static void TestFragmentRoundTrip(void) {
    IHS_Init();
    IHS_Session *session = IHS_TestSessionCreate();
    IHS_SessionChannel *channel = session->channels[IHS_SessionChannelIdControl];
    assert(channel != NULL);

    const size_t mtu = session->state.mtu > 0 ? session->state.mtu : 1024;
    const size_t bodyLimit = mtu - IHS_PACKET_HEADER_SIZE;

    // Straddle every boundary: under, exactly at, and over one and two packets' worth.
    const size_t sizes[] = {1, bodyLimit - 1, bodyLimit, bodyLimit + 1, 2 * bodyLimit,
                            2 * bodyLimit + 1, 5 * bodyLimit - 3};
    for (size_t si = 0; si < sizeof(sizes) / sizeof(sizes[0]); si++) {
        const size_t size = sizes[si];
        uint8_t *payload = malloc(size);
        for (size_t i = 0; i < size; i++) {
            payload[i] = (uint8_t) (i * 31 + si);
        }

        IHS_SessionFrame frame;
        IHS_SessionChannelInitializePacketHeader(channel, &frame.header, IHS_SessionPacketTypeUnreliable, false,
                                                 IHS_PACKET_ID_NEXT);
        // Same shape as IHS_SessionPacketBodyInitialize — the reserved header prefix, which the
        // single-packet path hands straight to the packet and asserts on — but sized for the test
        // payload, since that helper caps bodies at 2048.
        IHS_BufferInit(&frame.body, IHS_PACKET_HEADER_SIZE + size, IHS_PACKET_HEADER_SIZE + size);
        IHS_BufferFillMem(&frame.body, 0, 0, IHS_PACKET_HEADER_SIZE);
        IHS_BufferOffsetBy(&frame.body, IHS_PACKET_HEADER_SIZE);
        IHS_BufferAppendMem(&frame.body, payload, size);
        assert(IHS_SessionChannelQueueFrame(channel, &frame, false));
        IHS_BufferClear(&frame.body, true);

        // Everything the sender produced, straight into a receiver's window.
        IHS_SessionPacketsWindow *window = IHS_SessionPacketsWindowCreate(64);
        size_t sentPackets = 0;
        for (IHS_QueueItem *item = IHS_QueuePoll(session->sendQueue); item != NULL;
             item = IHS_QueuePoll(session->sendQueue)) {
            QueuedPacket *queued = (QueuedPacket *) item;
            assert(IHS_SessionPacketsWindowAdd(window, &queued->packet));
            IHS_SessionPacketClear(&queued->packet, true);
            IHS_QueueItemFree(item);
            sentPackets++;
        }
        assert(sentPackets == (size + bodyLimit - 1) / bodyLimit);

        IHS_SessionFrame received;
        memset(&received, 0, sizeof(received));
        IHS_BufferInit(&received.body, 64, 1024 * 1024);
        assert(IHS_SessionPacketsWindowPoll(window, &received));
        assert(received.body.size == size);
        assert(memcmp(IHS_BufferPointer(&received.body), payload, size) == 0);
        // The whole frame was consumed; nothing is left waiting for a fragment that never comes.
        assert(IHS_SessionPacketsWindowSize(window) == 0);
        IHS_SessionPacketsWindowReleaseFrame(&received);

        IHS_BufferClear(&received.body, true);
        IHS_SessionPacketsWindowDestroy(window);
        free(payload);
    }

    IHS_SessionDestroy(session);
    IHS_Quit();
}

int main(void) {
    TestSingleFragment();
    TestTwoFragments();
    TestIncompleteWaits();
    TestZeroCountDoesNotSpin();
    TestNegativeCountDoesNotCorrupt();
    TestSkipsRunOfMalformedHeads();
    TestFragmentRoundTrip();
    printf("window tests OK\n");
    return 0;
}
