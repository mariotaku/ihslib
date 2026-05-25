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

// Regression test for the DisconnectTimerEnd UAF (commit c516f06).
//
// Without the fix, IHS_SessionChannelDiscoveryDisconnect schedules a timer task,
// and IHS_SessionDestroy frees the discovery channel before the timer is stopped.
// The timer thread eventually fires DisconnectTimerEnd on the freed channel,
// writing through `discoveryChannel->disconnectTimerTask` and calling
// OnDisconnect on the freed channel + session.
//
// The fix calls IHS_TimerTaskStopImmediate from OnDiscoveryDeinit so the timer
// task is removed (and its end callback fired) synchronously while the channel
// is still valid.
//
// This test exercises both the immediate destroy path (no time for the timer to
// fire) and a destroy-after-brief-sleep path (timer is mid-execution). Run under
// Valgrind to catch any heap-use-after-free.

#include <unistd.h>
#include <assert.h>
#include <stdio.h>

#include "session/session_pri.h"
#include "session/channels/channel.h"
#include "session/channels/ch_discovery.h"
#include "test_session.h"

static void test_destroy_immediately_after_disconnect(void) {
    IHS_Session *session = IHS_TestSessionCreate();
    IHS_SessionChannel *discovery = IHS_SessionChannelFor(session, IHS_SessionChannelIdDiscovery);
    IHS_SessionChannelDiscoveryDisconnect(discovery);
    // No sleep: race the destroy against the timer thread's first poll. The fix
    // (StopImmediate in OnDiscoveryDeinit) removes the task before Destroy frees
    // the channel.
    IHS_SessionDestroy(session);
}

static void test_destroy_after_timer_starts_firing(void) {
    IHS_Session *session = IHS_TestSessionCreate();
    IHS_SessionChannel *discovery = IHS_SessionChannelFor(session, IHS_SessionChannelIdDiscovery);
    IHS_SessionChannelDiscoveryDisconnect(discovery);
    // Sleep long enough for the timer thread to enter TaskExecute. The timer thread
    // polls every ~1 ms; 20 ms guarantees at least one fire of DisconnectTimerRun
    // (which would otherwise reschedule every 100 ms). StopImmediate must still
    // remove the task cleanly under contention with the worker thread.
    usleep(20 * 1000);
    IHS_SessionDestroy(session);
}

int main(void) {
    IHS_Init();
    for (int i = 0; i < 5; ++i) {
        test_destroy_immediately_after_disconnect();
    }
    for (int i = 0; i < 5; ++i) {
        test_destroy_after_timer_starts_firing();
    }
    IHS_Quit();
    printf("disconnect-destroy UAF tests OK\n");
    return 0;
}
