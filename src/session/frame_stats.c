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

#include "frame_stats.h"

#include <assert.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "session_pri.h"

/* EStreamFrameEvent ids that drive the accumulator. Mirrors Steam's
 * AddFrameSample event references — used as indices into events[]. */
enum {
    EV_INPUT_START = 0,
    EV_INPUT_SEND = 1,
    EV_INPUT_RECV = 2,
    EV_FRAME_START = 5,
    EV_CAPTURE_BEGIN = 6,
    EV_CAPTURE_END = 7,
    EV_CONVERT_BEGIN = 8,
    EV_CONVERT_END = 9,
    EV_ENCODE_BEGIN = 10,
    EV_ENCODE_END = 11,
    EV_FRAME_SEND = 12,
    EV_FRAME_RECV = 13,
    EV_DECODE_BEGIN = 14,
    EV_DECODE_END = 15,
    EV_UPLOAD_BEGIN = 16,
    EV_UPLOAD_END = 17,
    EV_COMPLETE = 18,
};

/* Convert a delta in 1/65536-second ticks to milliseconds (float).
 * Matches Steam's GetStreamTimestampDeltaMS arithmetic. Wrapped subtraction
 * keeps timestamps that crossed the uint32 boundary correct (they never will
 * in practice — 65536 wraps every ~18 hours — but the cost is one cast). */
static float ticks_to_ms(uint32_t earlier, uint32_t later) {
    uint32_t delta = later - earlier;
    return (float) (delta * 1000.0 / 65536.0);
}

IHS_FrameStatsAggregator *IHS_FrameStatsAggregatorCreate(void) {
    IHS_FrameStatsAggregator *agg = calloc(1, sizeof(*agg));
    if (agg == NULL) {
        return NULL;
    }
    agg->lock = IHS_MutexCreate();
    if (agg->lock == NULL) {
        free(agg);
        return NULL;
    }
    return agg;
}

void IHS_FrameStatsAggregatorDestroy(IHS_FrameStatsAggregator *agg) {
    if (agg == NULL) {
        return;
    }
    IHS_MutexDestroy(agg->lock);
    free(agg);
}

void IHS_FrameStatsAggregatorSetFullReporting(IHS_FrameStatsAggregator *agg, bool enabled) {
    IHS_MutexLock(agg->lock);
    agg->fullReporting = enabled;
    IHS_MutexUnlock(agg->lock);
}

static IHS_FrameStatsSlot *acquire_slot(IHS_FrameStatsAggregator *agg, uint16_t frameId) {
    IHS_FrameStatsSlot *slot = &agg->ring[frameId & (IHS_FRAME_STATS_RING_SIZE - 1)];
    if (!slot->inUse || slot->frameId != frameId) {
        memset(slot, 0, sizeof(*slot));
        slot->frameId = frameId;
        slot->inUse = true;
    }
    return slot;
}

static void set_event(IHS_FrameStatsSlot *slot, uint32_t eventId, uint32_t timestamp) {
    assert(eventId < IHS_FRAME_STATS_EVENT_COUNT);
    slot->events[eventId] = timestamp;
    slot->eventMask |= (1u << eventId);
}

static bool has_event(const IHS_FrameStatsSlot *slot, uint32_t eventId) {
    return (slot->eventMask & (1u << eventId)) != 0;
}

void IHS_FrameStatsRecordStage(IHS_FrameStatsAggregator *agg, uint16_t frameId,
                               IHS_VideoFrameStage stage, uint32_t timestamp) {
    /* IHS_VideoFrameStage values are deliberately equal to EStreamFrameEvent ids. */
    uint32_t ev = (uint32_t) stage;
    if (ev >= IHS_FRAME_STATS_EVENT_COUNT) {
        return;
    }
    IHS_MutexLock(agg->lock);
    IHS_FrameStatsSlot *slot = acquire_slot(agg, frameId);
    set_event(slot, ev, timestamp);
    IHS_MutexUnlock(agg->lock);
}

void IHS_FrameStatsRecordComplete(IHS_FrameStatsAggregator *agg, uint16_t frameId,
                                  IHS_VideoFrameResult result) {
    IHS_MutexLock(agg->lock);
    IHS_FrameStatsSlot *slot = acquire_slot(agg, frameId);
    slot->result = result;
    slot->complete = true;
    /* Only displayed frames advance the cursor — matches Steam's
     * RecordFrameComplete, where IsDisplayed() (result == 1) gates the
     * lastDisplayedFrameId write. Dropped frames sit in the ring waiting
     * for the next displayed frame to draw a boundary, then get folded
     * in the drain pass as part of the [lastSent, lastDisplayed] range. */
    if (result == IHS_VideoFrameResultDisplayed) {
        agg->lastDisplayedFrameId = frameId;
    }
    IHS_MutexUnlock(agg->lock);
}

void IHS_FrameStatsRecordReceived(IHS_FrameStatsAggregator *agg, uint16_t frameId,
                                  uint32_t senderFrameTimestamp,
                                  uint32_t senderSendTimestamp,
                                  uint32_t recvTimestamp,
                                  uint32_t frameSize,
                                  uint16_t inputMark) {
    IHS_MutexLock(agg->lock);
    IHS_FrameStatsSlot *slot = &agg->ring[frameId & (IHS_FRAME_STATS_RING_SIZE - 1)];
    if (!slot->inUse || slot->frameId != frameId) {
        memset(slot, 0, sizeof(*slot));
        slot->frameId = frameId;
        slot->inUse = true;
        slot->frameSize = frameSize;
        slot->inputMark = inputMark;
        set_event(slot, EV_FRAME_START, senderFrameTimestamp);
        set_event(slot, EV_FRAME_SEND, senderSendTimestamp);
        set_event(slot, EV_FRAME_RECV, recvTimestamp);
    } else {
        /* Additional partial — only refresh recv timestamp. */
        set_event(slot, EV_FRAME_RECV, recvTimestamp);
    }
    IHS_MutexUnlock(agg->lock);
}

static void accum_add(IHS_FrameStatsAccumulator *accum, int statId, double value) {
    if (statId < 0 || statId >= IHS_FRAME_STATS_ACCUM_SLOTS) {
        return;
    }
    accum->slots[statId].count++;
    accum->slots[statId].sum += value;
    accum->slots[statId].sumSquares += value * value;
}

void IHS_FrameStatsAccumulatorReset(IHS_FrameStatsAccumulator *accum) {
    memset(accum, 0, sizeof(*accum));
}

/* Fold one completed slot into the accumulator. Slot lock held by caller. */
static void fold_slot(IHS_FrameStatsAggregator *agg, IHS_FrameStatsSlot *slot) {
    IHS_FrameStatsAccumulator *accum = &agg->accumulator;

    /* Inter-frame interval (stat 10) — needs the previous frame's event 18. */
    if (has_event(slot, EV_COMPLETE) && agg->lastFrameTimestamp != 0) {
        accum_add(accum, 10 /* FrameDurationMS */,
                  ticks_to_ms(agg->lastFrameTimestamp, slot->events[EV_COMPLETE]));
    }
    if (has_event(slot, EV_COMPLETE)) {
        agg->lastFrameTimestamp = slot->events[EV_COMPLETE];
    }

    /* Network duration (stat 6): event 12 → 13. */
    if (has_event(slot, EV_FRAME_SEND) && has_event(slot, EV_FRAME_RECV)) {
        accum_add(accum, 6,
                  ticks_to_ms(slot->events[EV_FRAME_SEND], slot->events[EV_FRAME_RECV]));
    }
    /* Decode duration (stat 7): event 14 → 15. */
    if (has_event(slot, EV_DECODE_BEGIN) && has_event(slot, EV_DECODE_END)) {
        accum_add(accum, 7,
                  ticks_to_ms(slot->events[EV_DECODE_BEGIN], slot->events[EV_DECODE_END]));
    }
    /* Display / upload duration (stat 8): event 16 → 17. */
    if (has_event(slot, EV_UPLOAD_BEGIN) && has_event(slot, EV_UPLOAD_END)) {
        accum_add(accum, 8,
                  ticks_to_ms(slot->events[EV_UPLOAD_BEGIN], slot->events[EV_UPLOAD_END]));
    }
    /* Client end-to-end (stat 9): event 13 → 18. */
    if (has_event(slot, EV_FRAME_RECV) && has_event(slot, EV_COMPLETE)) {
        accum_add(accum, 9,
                  ticks_to_ms(slot->events[EV_FRAME_RECV], slot->events[EV_COMPLETE]));
    }

    /* Stats 0 (FPS), 11/12/13 (input latency variants), 14 (ping), 15/16/17
     * (bitrates), 18 (loss) require either an FPS sampler, the input-mark
     * pipeline, or network-stats hooks that ihslib does not yet own. They are
     * left out of this MVP; when those signals land they can fold in here. */
}

size_t IHS_FrameStatsAggregatorDrain(IHS_FrameStatsAggregator *agg, uint16_t *outLatestFrameId) {
    size_t folded = 0;
    IHS_MutexLock(agg->lock);

    uint16_t lastSent = agg->lastSentFrameId;
    uint16_t lastDisplayed = agg->lastDisplayedFrameId;
    while (lastSent != lastDisplayed) {
        uint16_t frameId = (uint16_t) (lastSent + 1);
        IHS_FrameStatsSlot *slot = &agg->ring[frameId & (IHS_FRAME_STATS_RING_SIZE - 1)];
        if (slot->inUse && slot->frameId == frameId) {
            if (!slot->complete) {
                /* Mark as DroppedLate so the host sees the slot was processed
                 * even though the app never called RecordComplete in time. */
                slot->result = IHS_VideoFrameResultDroppedLate;
                slot->complete = true;
            }
            fold_slot(agg, slot);
            memset(slot, 0, sizeof(*slot));
            folded++;
        }
        lastSent = frameId;
    }

    agg->lastSentFrameId = lastSent;
    if (outLatestFrameId != NULL) {
        *outLatestFrameId = lastSent;
    }
    IHS_MutexUnlock(agg->lock);
    return folded;
}
