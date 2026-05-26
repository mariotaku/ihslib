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

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "ihslib/session.h"
#include "ihs_thread.h"

/**
 * Per-frame timestamp + result record. Mirrors Steam's CFastFrameStats:
 * a fixed-size slot in a 128-entry ring keyed by frameId & 0x7f. Each frame
 * carries up to 19 event timestamps (one per EStreamFrameEvent), the size of
 * the frame data, and the final IHS_VideoFrameResult.
 *
 * Timestamps are in IHS_SessionPacketTimestamp units (1/65536 second).
 *
 * `eventMask` bit i is set when `events[i]` has been recorded — distinguishes
 * "not yet observed" from "recorded as zero" (frameId 0 / frame 0 / etc).
 */
#define IHS_FRAME_STATS_EVENT_COUNT 19

typedef struct IHS_FrameStatsSlot {
    uint16_t frameId;
    uint32_t eventMask;             /* bit i set iff events[i] is valid */
    uint32_t events[IHS_FRAME_STATS_EVENT_COUNT];
    uint32_t frameSize;             /* bytes; 0 if unknown */
    uint16_t inputMark;             /* 0 if not bound to an input mark */
    IHS_VideoFrameResult result;
    bool inUse;
    bool complete;                  /* event 18 (Complete) recorded */
} IHS_FrameStatsSlot;

/**
 * 19-slot aggregator matching CFrameStatsAccumulator. Each slot tracks
 * (count, sum, sum_of_squares) over a flush window so we can produce
 * (count, average, stddev) at flush time via the same closed-form Steam uses.
 */
#define IHS_FRAME_STATS_ACCUM_SLOTS 19

typedef struct IHS_FrameStatsAccumSlot {
    uint32_t count;
    double sum;
    double sumSquares;
} IHS_FrameStatsAccumSlot;

typedef struct IHS_FrameStatsAccumulator {
    IHS_FrameStatsAccumSlot slots[IHS_FRAME_STATS_ACCUM_SLOTS];
} IHS_FrameStatsAccumulator;

/**
 * 128-slot ring + lock + accumulator. One per session. All accessors take
 * `lock` internally; callers must not hold any other session lock when
 * invoking them to avoid lock-order inversion.
 */
#define IHS_FRAME_STATS_RING_SIZE 128

typedef struct IHS_FrameStatsAggregator {
    IHS_Mutex *lock;
    IHS_FrameStatsSlot ring[IHS_FRAME_STATS_RING_SIZE];
    IHS_FrameStatsAccumulator accumulator;
    uint16_t lastSentFrameId;
    uint16_t lastDisplayedFrameId;
    uint32_t lastFrameTimestamp;    /* event 18 of the previous *completed* frame */
    bool fullReporting;             /* mirrors CStreamClient::sendFullFrameStats */
} IHS_FrameStatsAggregator;

IHS_FrameStatsAggregator *IHS_FrameStatsAggregatorCreate(void);

void IHS_FrameStatsAggregatorDestroy(IHS_FrameStatsAggregator *agg);

void IHS_FrameStatsAggregatorSetFullReporting(IHS_FrameStatsAggregator *agg, bool enabled);

/**
 * Record one of the four app-driven events (DecodeBegin/DecodeEnd/UploadBegin/UploadEnd).
 * `timestamp` is in 1/65536-second units; `0` means "use IHS_SessionTimestampNow".
 *
 * Ring-slot policy: if the slot's frameId doesn't match, the slot is reset
 * and the new frameId takes ownership (i.e. late events for a wrapped-out
 * frameId are dropped, which matches Steam's behavior — the ring just
 * forgets ancient frames).
 */
void IHS_FrameStatsRecordStage(IHS_FrameStatsAggregator *agg, uint16_t frameId,
                               IHS_VideoFrameStage stage, uint32_t timestamp);

/**
 * Record the final result of a frame. Sets event 18 (Complete). Computes
 * inter-frame interval from the previous completed frame's event 18, so this
 * MUST be called once per frame the app processed (whether displayed or
 * dropped) for the FPS / Frame-duration stats to be meaningful.
 */
void IHS_FrameStatsRecordComplete(IHS_FrameStatsAggregator *agg, uint16_t frameId,
                                  IHS_VideoFrameResult result);

/**
 * Internal hook for the video-receive path. Seeds events 5 (Start), 12 (Send),
 * 13 (Recv) on first observation of a frame; subsequent partial-receives only
 * refresh event 13. `frameSize` is from the IHS_VideoFrameHeader.
 */
void IHS_FrameStatsRecordReceived(IHS_FrameStatsAggregator *agg, uint16_t frameId,
                                  uint32_t senderFrameTimestamp,
                                  uint32_t senderSendTimestamp,
                                  uint32_t recvTimestamp,
                                  uint32_t frameSize,
                                  uint16_t inputMark);

/**
 * Drain the ring for everything between lastSentFrameId and lastDisplayedFrameId
 * (exclusive→inclusive), fold completed slots into the accumulator, and reset
 * them. Returns the count of slots folded; the accumulator and the returned
 * `latestFrameId` are the inputs for one CFrameStatsListMsg flush.
 *
 * Caller is expected to call IHS_FrameStatsAccumulatorReset on the accumulator
 * AFTER serializing the flush packet.
 */
size_t IHS_FrameStatsAggregatorDrain(IHS_FrameStatsAggregator *agg, uint16_t *outLatestFrameId);

void IHS_FrameStatsAccumulatorReset(IHS_FrameStatsAccumulator *accum);
