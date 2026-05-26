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

#include <assert.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "ihslib.h"
#include "session/frame_stats.h"

/* EFrameAccumulatedStat slot ids that this MVP populates. */
enum {
    SLOT_NETWORK = 6,
    SLOT_DECODE = 7,
    SLOT_DISPLAY = 8,
    SLOT_CLIENT = 9,
    SLOT_FRAME_DURATION = 10,
};

/* Helper: drain returns folded count, accumulator holds the sums. */
static void expect_slot(const IHS_FrameStatsAggregator *agg, int slot, uint32_t count, float average) {
    assert(agg->accumulator.slots[slot].count == count);
    if (count > 0) {
        float got = (float) (agg->accumulator.slots[slot].sum / agg->accumulator.slots[slot].count);
        float diff = got - average;
        if (diff < 0) diff = -diff;
        assert(diff < 0.5f);  /* 1/65536-s → ms quantisation slop */
    }
}

/* 1 ms in IHS_SessionPacketTimestamp ticks (1/65536-second). */
#define MS_TICKS(ms) ((uint32_t)((ms) * 65536u / 1000u))

int main(void) {
    IHS_FrameStatsAggregator *agg = IHS_FrameStatsAggregatorCreate();
    assert(agg != NULL);

    /* Frame 1: full pipeline. Each stage 10 ms apart. */
    /*   Receive (events 5/12/13)                  at  0 ms */
    IHS_FrameStatsRecordReceived(agg, 1, MS_TICKS(0), MS_TICKS(0), MS_TICKS(0), 1024, 0);
    /*   DecodeBegin (14)                          at 10 ms */
    IHS_FrameStatsRecordStage(agg, 1, IHS_VideoFrameStageDecodeBegin, MS_TICKS(10));
    /*   DecodeEnd (15)                            at 20 ms */
    IHS_FrameStatsRecordStage(agg, 1, IHS_VideoFrameStageDecodeEnd, MS_TICKS(20));
    /*   UploadBegin (16)                          at 20 ms */
    IHS_FrameStatsRecordStage(agg, 1, IHS_VideoFrameStageUploadBegin, MS_TICKS(20));
    /*   UploadEnd (17)                            at 25 ms */
    IHS_FrameStatsRecordStage(agg, 1, IHS_VideoFrameStageUploadEnd, MS_TICKS(25));
    /*   Complete (18) at 30 ms, Displayed. The public-API wrapper would have
     *   stamped event 18 + advanced lastDisplayedFrameId; here we replay both
     *   explicit steps. */
    IHS_FrameStatsRecordStage(agg, 1, (IHS_VideoFrameStage) 18, MS_TICKS(30));
    IHS_FrameStatsRecordComplete(agg, 1, IHS_VideoFrameResultDisplayed);

    /* Frame 2: inter-frame interval test. Complete at 50 ms (20 ms after frame 1). */
    IHS_FrameStatsRecordReceived(agg, 2, MS_TICKS(20), MS_TICKS(20), MS_TICKS(20), 1024, 0);
    IHS_FrameStatsRecordStage(agg, 2, IHS_VideoFrameStageDecodeBegin, MS_TICKS(30));
    IHS_FrameStatsRecordStage(agg, 2, IHS_VideoFrameStageDecodeEnd, MS_TICKS(40));
    IHS_FrameStatsRecordStage(agg, 2, IHS_VideoFrameStageUploadBegin, MS_TICKS(40));
    IHS_FrameStatsRecordStage(agg, 2, IHS_VideoFrameStageUploadEnd, MS_TICKS(45));
    IHS_FrameStatsRecordStage(agg, 2, (IHS_VideoFrameStage) 18, MS_TICKS(50));
    IHS_FrameStatsRecordComplete(agg, 2, IHS_VideoFrameResultDisplayed);

    /* Drain: two displayed frames. lastSent was 0, lastDisplayed advanced to 2,
     * so we should fold both. */
    uint16_t latest = 0;
    size_t folded = IHS_FrameStatsAggregatorDrain(agg, &latest);
    assert(folded == 2);
    assert(latest == 2);

    /* Decode: each frame is 10 ms (10→20 and 30→40). avg ≈ 10 ms. */
    expect_slot(agg, SLOT_DECODE, 2, 10.0f);
    /* Display/upload: 5 ms each (20→25 and 40→45). avg ≈ 5 ms. */
    expect_slot(agg, SLOT_DISPLAY, 2, 5.0f);
    /* Network: event 12→13 — same timestamp seeded, delta = 0 ms. avg ≈ 0. */
    expect_slot(agg, SLOT_NETWORK, 2, 0.0f);
    /* Client end-to-end: 13→18 — 30 ms each (0→30 and 20→50). avg ≈ 30 ms. */
    expect_slot(agg, SLOT_CLIENT, 2, 30.0f);
    /* Frame duration: only one inter-frame interval (frame 1's 18 → frame 2's 18 = 20 ms).
     * Frame 1 itself has no predecessor so it doesn't add a sample. count=1. */
    expect_slot(agg, SLOT_FRAME_DURATION, 1, 20.0f);

    /* Frames between lastSent and lastDisplayed that NEVER received Complete must
     * be folded as DroppedLate so the slot is reset and not leaked. Inject a
     * dropped frame between two displayed ones. */
    IHS_FrameStatsAccumulatorReset(&agg->accumulator);

    IHS_FrameStatsRecordReceived(agg, 3, MS_TICKS(40), MS_TICKS(40), MS_TICKS(40), 1024, 0);
    /* No stages — frame 3 was dropped by the app before reaching decode. */
    IHS_FrameStatsRecordReceived(agg, 4, MS_TICKS(50), MS_TICKS(50), MS_TICKS(50), 1024, 0);
    IHS_FrameStatsRecordStage(agg, 4, IHS_VideoFrameStageDecodeBegin, MS_TICKS(60));
    IHS_FrameStatsRecordStage(agg, 4, IHS_VideoFrameStageDecodeEnd, MS_TICKS(70));
    IHS_FrameStatsRecordStage(agg, 4, (IHS_VideoFrameStage) 18, MS_TICKS(75));
    IHS_FrameStatsRecordComplete(agg, 4, IHS_VideoFrameResultDisplayed);

    folded = IHS_FrameStatsAggregatorDrain(agg, &latest);
    assert(folded == 2);                  /* frame 3 (dropped late) and frame 4 (displayed) */
    assert(latest == 4);
    /* Frame 3 had no decode/network events so contributes only its (absent) slot
     * entries; frame 4 contributes one Decode (10 ms) and no Network (delta=0). */
    expect_slot(agg, SLOT_DECODE, 1, 10.0f);

    /* Dropped-only frames don't advance the cursor. After this, lastSent should
     * equal lastDisplayed (4), so a subsequent drain folds nothing. */
    folded = IHS_FrameStatsAggregatorDrain(agg, &latest);
    assert(folded == 0);

    /* Frames between cursors that the app drops (result != Displayed) should
     * still be folded — they sit in the [lastSent, lastDisplayed] range when
     * the next Displayed frame draws a boundary. */
    IHS_FrameStatsAccumulatorReset(&agg->accumulator);
    IHS_FrameStatsRecordReceived(agg, 5, MS_TICKS(80), MS_TICKS(80), MS_TICKS(80), 1024, 0);
    IHS_FrameStatsRecordStage(agg, 5, (IHS_VideoFrameStage) 18, MS_TICKS(95));
    IHS_FrameStatsRecordComplete(agg, 5, IHS_VideoFrameResultDroppedDecodeSlow);
    /* Frame 5 dropped → lastDisplayedFrameId still 4 → drain folds nothing yet. */
    folded = IHS_FrameStatsAggregatorDrain(agg, &latest);
    assert(folded == 0);
    /* Frame 6 displayed → boundary drawn at 6, frame 5 is in the range and gets folded. */
    IHS_FrameStatsRecordReceived(agg, 6, MS_TICKS(90), MS_TICKS(90), MS_TICKS(90), 1024, 0);
    IHS_FrameStatsRecordStage(agg, 6, (IHS_VideoFrameStage) 18, MS_TICKS(110));
    IHS_FrameStatsRecordComplete(agg, 6, IHS_VideoFrameResultDisplayed);
    folded = IHS_FrameStatsAggregatorDrain(agg, &latest);
    assert(folded == 2);                  /* frames 5 and 6 */
    assert(latest == 6);

    IHS_FrameStatsAggregatorDestroy(agg);
    return 0;
}
