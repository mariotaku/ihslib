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
 * The per-frame statistics ring: slot claiming, the conditions under which a result is dropped, the
 * drain window that chases the last displayed frame, and the accumulated values that go out on the
 * stats channel once a second.
 */

#include <assert.h>
#include <math.h>
#include <string.h>

#include "session/channels/video/frame_stats.h"

/** One millisecond in the wire's 1/65536-second units. */
#define MS(x) ((uint32_t) ((x) * 65536 / 1000))

static void Receive(IHS_VideoFrameStatsRing *ring, uint16_t frameId, uint32_t sent, uint32_t received) {
    IHS_VideoFrameStatsInfo info = {
            .frameId = frameId,
            .frameTimestamp = sent,
            .sendTimestamp = sent,
            .recvTimestamp = received,
    };
    IHS_VideoFrameStatsReceived(ring, &info);
}

static const CFrameStatAccumulatedValue *FindStat(const CFrameStatsListMsg *message, EFrameAccumulatedStat type) {
    for (size_t i = 0; i < message->n_accumulated_stats; i++) {
        if (message->accumulated_stats[i]->stat_type == type) {
            return message->accumulated_stats[i];
        }
    }
    return NULL;
}

static void TestClaimAndComplete(void) {
    IHS_VideoFrameStatsRing ring;
    IHS_VideoFrameStatsRingInit(&ring);

    // A result for a frame that was never received has no slot to land in.
    assert(!IHS_VideoFrameStatsComplete(&ring, 7, k_EStreamFrameResultDisplayed, MS(100)));

    Receive(&ring, 7, MS(10), MS(20));
    assert(IHS_VideoFrameStatsComplete(&ring, 7, k_EStreamFrameResultDisplayed, MS(30)));
    // Already resolved: RecordFrameComplete @ 0x1fafc8 refuses to overwrite a result.
    assert(!IHS_VideoFrameStatsComplete(&ring, 7, k_EStreamFrameResultDroppedLate, MS(40)));
    assert(ring.lastDisplayedFrameId == 7);

    // Only a displayed frame moves the display chain along.
    Receive(&ring, 8, MS(26), MS(36));
    assert(IHS_VideoFrameStatsComplete(&ring, 8, k_EStreamFrameResultDroppedReset, MS(46)));
    assert(ring.lastDisplayedFrameId == 7);

    // The ring holds 128 frames, so frame 7 + 128 evicts frame 7's slot and the stale id is refused.
    Receive(&ring, 7 + IHS_VIDEO_FRAME_STATS_RING_SIZE, MS(50), MS(60));
    assert(!IHS_VideoFrameStatsComplete(&ring, 7, k_EStreamFrameResultDisplayed, MS(70)));
    assert(IHS_VideoFrameStatsComplete(&ring, 7 + IHS_VIDEO_FRAME_STATS_RING_SIZE,
                                       k_EStreamFrameResultDisplayed, MS(70)));
}

static void TestSkippedFrameGap(void) {
    IHS_VideoFrameStatsRing ring;
    IHS_VideoFrameStatsRingInit(&ring);

    // The range is half-open: 11..14 are reported, neither 10 nor 15.
    for (uint16_t id = 11; id <= 15; id++) {
        Receive(&ring, id, MS(id), MS(id + 5));
    }
    assert(IHS_VideoFrameStatsGapSize(10, 15) == 4);
    assert(IHS_VideoFrameStatsReportSkipped(&ring, 10, 15, MS(100)) == 4);
    // 15 stayed pending, so it can still be resolved as displayed.
    assert(IHS_VideoFrameStatsComplete(&ring, 15, k_EStreamFrameResultDisplayed, MS(101)));
    // ...while 11 was already resolved by the gap fill.
    assert(!IHS_VideoFrameStatsComplete(&ring, 11, k_EStreamFrameResultDisplayed, MS(101)));

    // Adjacent ids leave no gap at all.
    assert(IHS_VideoFrameStatsGapSize(20, 21) == 0);
    assert(IHS_VideoFrameStatsReportSkipped(&ring, 20, 21, MS(110)) == 0);

    // Ids wrap: the gap between 65530 and 3 is the eight ids in between.
    assert(IHS_VideoFrameStatsGapSize(65530, 3) == 8);

    // A keyframe at or behind the last completed frame would walk the whole 16-bit space in the
    // reference. Nothing is reported instead of ~65000 events.
    assert(IHS_VideoFrameStatsGapSize(100, 100) == 65535);
    assert(IHS_VideoFrameStatsReportSkipped(&ring, 100, 100, MS(120)) == 0);
    assert(IHS_VideoFrameStatsGapSize(100, 50) == 65485);
    assert(IHS_VideoFrameStatsReportSkipped(&ring, 100, 50, MS(120)) == 0);
    // The widest gap still worth walking is one full ring.
    assert(IHS_VideoFrameStatsReportSkipped(&ring, 200, 200 + IHS_VIDEO_FRAME_STATS_RING_SIZE + 1, MS(130)) == 0);
}

static void TestDrainWindow(void) {
    IHS_VideoFrameStatsRing ring;
    IHS_VideoFrameStatsRingInit(&ring);
    IHS_FrameStatsAccumulator accumulator;
    IHS_FrameStatsAccumulatorInit(&accumulator);

    // Nothing displayed yet: the window is empty even though frames have arrived.
    Receive(&ring, 1, MS(0), MS(5));
    Receive(&ring, 2, MS(16), MS(21));
    assert(IHS_VideoFrameStatsDrain(&ring, &accumulator, MS(100)) == 0);

    // Frame 3 displays; 1 and 2 are inside the window and were never resolved, so they are drained
    // as DroppedLate the way SendFrameEvents @ 0x1fb5f4 rewrites them.
    Receive(&ring, 3, MS(32), MS(37));
    assert(IHS_VideoFrameStatsComplete(&ring, 3, k_EStreamFrameResultDisplayed, MS(40)));
    assert(IHS_VideoFrameStatsDrain(&ring, &accumulator, MS(100)) == 3);
    assert(ring.sendCursor == 3);

    // Draining destroys the records, so a second pass finds nothing.
    assert(IHS_VideoFrameStatsDrain(&ring, &accumulator, MS(110)) == 0);

    // Ids that never arrived are stepped over rather than reported.
    Receive(&ring, 8, MS(48), MS(53));
    assert(IHS_VideoFrameStatsComplete(&ring, 8, k_EStreamFrameResultDisplayed, MS(56)));
    assert(IHS_VideoFrameStatsDrain(&ring, &accumulator, MS(120)) == 1);
    assert(ring.sendCursor == 8);
}

static void TestAccumulatedStats(void) {
    IHS_VideoFrameStatsRing ring;
    IHS_VideoFrameStatsRingInit(&ring);
    IHS_FrameStatsAccumulator accumulator;
    IHS_FrameStatsAccumulatorInit(&accumulator);

    // Three frames, each exactly 10 ms on the wire and each handed on 5 ms after it arrived.
    for (uint16_t id = 1; id <= 3; id++) {
        uint32_t sent = MS(id * 16);
        Receive(&ring, id, sent, sent + MS(10));
        assert(IHS_VideoFrameStatsComplete(&ring, id, k_EStreamFrameResultDisplayed, sent + MS(15)));
    }
    assert(IHS_VideoFrameStatsDrain(&ring, &accumulator, MS(100)) == 3);

    CFrameStatsListMsg message = CFRAME_STATS_LIST_MSG__INIT;
    assert(IHS_FrameStatsAccumulatorFill(&accumulator, &message));

    const CFrameStatAccumulatedValue *network = FindStat(&message, k_EFrameStatNetworkDurationMS);
    assert(network != NULL);
    assert(network->count == 3);
    assert(fabsf(network->average - 10.0f) < 0.1f);
    // Every sample is identical, so there is no deviation to report.
    assert(!network->has_stddev);

    const CFrameStatAccumulatedValue *client = FindStat(&message, k_EFrameStatClientDurationMS);
    assert(client != NULL);
    assert(fabsf(client->average - 5.0f) < 0.1f);

    // No input mark was echoed back, so the latency stats have no samples at all.
    assert(FindStat(&message, k_EFrameStatInputLatencyMS) == NULL);
    assert(FindStat(&message, k_EFrameStatRoundTripLatencyMS) == NULL);
    // Decode and display timings live in the application's decoder and are never sampled here.
    assert(FindStat(&message, k_EFrameStatDecodeDurationMS) == NULL);
    assert(FindStat(&message, k_EFrameStatDisplayDurationMS) == NULL);

    IHS_FrameStatsListClear(&message);
    assert(message.n_accumulated_stats == 0);

    // An empty accumulator has nothing to send.
    IHS_FrameStatsAccumulatorInit(&accumulator);
    CFrameStatsListMsg empty = CFRAME_STATS_LIST_MSG__INIT;
    assert(!IHS_FrameStatsAccumulatorFill(&accumulator, &empty));
}

static void TestInputLatencyStats(void) {
    IHS_VideoFrameStatsRing ring;
    IHS_VideoFrameStatsRingInit(&ring);
    IHS_FrameStatsAccumulator accumulator;
    IHS_FrameStatsAccumulatorInit(&accumulator);

    // A frame reflecting an input the host acknowledged 8 ms after the event happened.
    IHS_VideoFrameStatsInfo info = {
            .frameId = 1,
            .frameTimestamp = MS(20),
            .sendTimestamp = MS(20),
            .recvTimestamp = MS(30),
            .inputMark = 42,
            .hasInputMark = true,
            .inputEventTimestamp = MS(0),
            .inputSendTimestamp = MS(2),
            .inputRecvTimestamp = MS(8),
    };
    IHS_VideoFrameStatsReceived(&ring, &info);
    assert(IHS_VideoFrameStatsComplete(&ring, 1, k_EStreamFrameResultDisplayed, MS(35)));
    assert(IHS_VideoFrameStatsDrain(&ring, &accumulator, MS(100)) == 1);

    CFrameStatsListMsg message = CFRAME_STATS_LIST_MSG__INIT;
    assert(IHS_FrameStatsAccumulatorFill(&accumulator, &message));

    const CFrameStatAccumulatedValue *input = FindStat(&message, k_EFrameStatInputLatencyMS);
    assert(input != NULL && fabsf(input->average - 8.0f) < 0.1f);
    // The host started the frame 12 ms after it received the input.
    const CFrameStatAccumulatedValue *game = FindStat(&message, k_EFrameStatGameLatencyMS);
    assert(game != NULL && fabsf(game->average - 12.0f) < 0.1f);
    // ...and the frame reached the application 35 ms after the input happened.
    const CFrameStatAccumulatedValue *roundTrip = FindStat(&message, k_EFrameStatRoundTripLatencyMS);
    assert(roundTrip != NULL && fabsf(roundTrip->average - 35.0f) < 0.1f);

    IHS_FrameStatsListClear(&message);
}

static void TestClockSkewRebase(void) {
    IHS_VideoFrameStatsRing ring;
    IHS_VideoFrameStatsRingInit(&ring);
    IHS_FrameStatsAccumulator accumulator;
    IHS_FrameStatsAccumulatorInit(&accumulator);

    // The host's clock reads 100 ms ahead of ours: the packet claims to have been sent after it
    // arrived. RecordFrameReceived @ 0x1fab7c shifts the host's timestamps back onto our clock, so
    // the network duration comes out as 0 rather than negative.
    IHS_VideoFrameStatsInfo info = {
            .frameId = 1,
            .frameTimestamp = MS(100),
            .sendTimestamp = MS(105),
            .recvTimestamp = MS(5),
    };
    IHS_VideoFrameStatsReceived(&ring, &info);
    assert(IHS_VideoFrameStatsComplete(&ring, 1, k_EStreamFrameResultDisplayed, MS(10)));
    assert(IHS_VideoFrameStatsDrain(&ring, &accumulator, MS(100)) == 1);

    CFrameStatsListMsg message = CFRAME_STATS_LIST_MSG__INIT;
    assert(IHS_FrameStatsAccumulatorFill(&accumulator, &message));
    const CFrameStatAccumulatedValue *network = FindStat(&message, k_EFrameStatNetworkDurationMS);
    assert(network != NULL && fabsf(network->average) < 0.1f);
    // The frame duration keeps the 5 ms the host spent between starting and sending the frame.
    const CFrameStatAccumulatedValue *frame = FindStat(&message, k_EFrameStatFrameDurationMS);
    assert(frame != NULL && fabsf(frame->average - 10.0f) < 0.1f);
    IHS_FrameStatsListClear(&message);
}

static void TestStandardDeviation(void) {
    IHS_VideoFrameStatsRing ring;
    IHS_VideoFrameStatsRingInit(&ring);
    IHS_FrameStatsAccumulator accumulator;
    IHS_FrameStatsAccumulatorInit(&accumulator);

    // Network durations of 10, 20 and 30 ms: mean 20, sample stddev 10.
    const uint32_t durations[] = {MS(10), MS(20), MS(30)};
    for (uint16_t id = 1; id <= 3; id++) {
        uint32_t sent = MS(id * 100);
        Receive(&ring, id, sent, sent + durations[id - 1]);
        assert(IHS_VideoFrameStatsComplete(&ring, id, k_EStreamFrameResultDisplayed, sent + MS(50)));
    }
    assert(IHS_VideoFrameStatsDrain(&ring, &accumulator, MS(1000)) == 3);

    CFrameStatsListMsg message = CFRAME_STATS_LIST_MSG__INIT;
    assert(IHS_FrameStatsAccumulatorFill(&accumulator, &message));
    const CFrameStatAccumulatedValue *network = FindStat(&message, k_EFrameStatNetworkDurationMS);
    assert(network != NULL);
    assert(fabsf(network->average - 20.0f) < 0.1f);
    assert(network->has_stddev);
    assert(fabsf(network->stddev - 10.0f) < 0.1f);
    IHS_FrameStatsListClear(&message);
}

int main() {
    TestClaimAndComplete();
    TestSkippedFrameGap();
    TestDrainWindow();
    TestAccumulatedStats();
    TestInputLatencyStats();
    TestClockSkewRebase();
    TestStandardDeviation();
    return 0;
}
