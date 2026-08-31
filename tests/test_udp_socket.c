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
 * The receive path is what lets the worker block indefinitely instead of polling, so its timeout and
 * wakeup semantics are load-bearing: get the wake wrong and an idle session can never be shut down.
 */

#include <assert.h>
#include <stdio.h>

#include "ihs_buffer.h"
#include "ihs_udp.h"
#include "ihs_timer.h"

/** A receive that finds nothing must return promptly, not block. */
static void test_zero_timeout_returns_immediately(void) {
    IHS_UDPSocket *socket = IHS_UDPSocketOpen(false);
    assert(socket != NULL);
    IHS_UDPPacket packet;
    IHS_BufferInit(&packet.buffer, 2048, 2048);

    uint64_t before = IHS_TimerNow();
    assert(IHS_UDPSocketReceive(socket, &packet, 0) == 0);
    uint64_t elapsed = IHS_TimerNow() - before;
    assert(elapsed < 50);

    IHS_BufferClear(&packet.buffer, true);
    IHS_UDPSocketClose(socket);
}

/** A timed receive waits roughly the requested time and then reports nothing arrived. */
static void test_timeout_waits(void) {
    IHS_UDPSocket *socket = IHS_UDPSocketOpen(false);
    IHS_UDPPacket packet;
    IHS_BufferInit(&packet.buffer, 2048, 2048);

    uint64_t before = IHS_TimerNow();
    assert(IHS_UDPSocketReceive(socket, &packet, 200) == 0);
    uint64_t elapsed = IHS_TimerNow() - before;
    // Generous either side: the point is that it waited rather than spun or hung.
    assert(elapsed >= 150);
    assert(elapsed < 2000);

    IHS_BufferClear(&packet.buffer, true);
    IHS_UDPSocketClose(socket);
}

/**
 * The one that matters for shutdown: a wake posted before the receive must still be seen, so an
 * interrupt can never be lost in the gap between setting the flag and entering the wait. Without it
 * this call would block forever on an idle socket and the test would time out.
 */
static void test_unblock_releases_an_indefinite_wait(void) {
    IHS_UDPSocket *socket = IHS_UDPSocketOpen(false);
    IHS_UDPPacket packet;
    IHS_BufferInit(&packet.buffer, 2048, 2048);

    assert(IHS_UDPSocketUnblock(socket));

    uint64_t before = IHS_TimerNow();
    assert(IHS_UDPSocketReceive(socket, &packet, -1) == 0);
    uint64_t elapsed = IHS_TimerNow() - before;
    assert(elapsed < 500);

    // The wake is consumed, so the socket is back to blocking normally.
    before = IHS_TimerNow();
    assert(IHS_UDPSocketReceive(socket, &packet, 100) == 0);
    assert(IHS_TimerNow() - before >= 50);

    IHS_BufferClear(&packet.buffer, true);
    IHS_UDPSocketClose(socket);
}

/** Repeated wakes must not accumulate into a receive that can never block again. */
static void test_repeated_unblocks_collapse(void) {
    IHS_UDPSocket *socket = IHS_UDPSocketOpen(false);
    IHS_UDPPacket packet;
    IHS_BufferInit(&packet.buffer, 2048, 2048);

    for (int i = 0; i < 64; i++) {
        assert(IHS_UDPSocketUnblock(socket));
    }
    assert(IHS_UDPSocketReceive(socket, &packet, -1) == 0);

    uint64_t before = IHS_TimerNow();
    assert(IHS_UDPSocketReceive(socket, &packet, 100) == 0);
    assert(IHS_TimerNow() - before >= 50 && "every queued wake should have been drained at once");

    IHS_BufferClear(&packet.buffer, true);
    IHS_UDPSocketClose(socket);
}

int main(void) {
    test_zero_timeout_returns_immediately();
    test_timeout_waits();
    test_unblock_releases_an_indefinite_wait();
    test_repeated_unblocks_collapse();
    printf("udp socket tests OK\n");
    return 0;
}
