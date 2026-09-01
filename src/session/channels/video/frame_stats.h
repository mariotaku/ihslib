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

/**
 * Per-frame video statistics, mirroring CFastFrameStats and the ClientFrameStats_t ring that
 * CStreamClient keeps at +0x28c.
 *
 * The reference records one 0x78-byte record per frame in a 128-entry ring indexed by
 * `frameId & 0x7f` (ClientFrameStats_t::GetFrameStats @ 0x1f50c8), claims a slot when the first
 * packet of a frame arrives (RecordFrameReceived @ 0x1faac4), resolves it when the frame's fate is
 * known (RecordFrameComplete @ 0x1fafc8), and drains resolved records into a CFrameStatsListMsg
 * once a second (SendFrameEvents @ 0x1fb4f0).
 *
 * Timestamps are all in IHS_SessionPacketTimestamp() units (1/65536 s).
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "protobuf/remoteplay.pb-c.h"

/** ClientFrameStats_t::GetFrameStats @ 0x1f50c8 is `this + (frameId & 0x7f) * 0x78 + 0xc`. */
#define IHS_VIDEO_FRAME_STATS_RING_SIZE 128

/** k_EStreamFrameEventComplete (18) is the highest event id. */
#define IHS_VIDEO_FRAME_EVENT_COUNT 19

/** EFrameAccumulatedStat runs k_EFrameStatFPS (0) .. k_EFrameStatPacketLossPercentage (18). */
#define IHS_FRAME_ACCUMULATED_STAT_COUNT 19

typedef struct IHS_VideoFrameStats {
    uint16_t frameId;
    /** 0 when the frame reflects no input, matching the host's own "no mark" sentinel. */
    uint16_t inputMark;
    /** k_EStreamFrameResultPending until the frame is resolved. */
    EStreamFrameResult result;
    /** Bit N set means events[N] holds a timestamp. Mirrors CFastFrameStats::HasEvent @ 0x209d10. */
    uint32_t eventMask;
    uint32_t events[IHS_VIDEO_FRAME_EVENT_COUNT];
    /** Milliseconds since the previous completed frame's FrameStart. CFastFrameStats+0x5c. */
    float frameStartDelta;
    /** Milliseconds since the previous displayed frame's Complete. CFastFrameStats+0x60. */
    float frameDisplayDelta;
} IHS_VideoFrameStats;

typedef struct IHS_VideoFrameStatsRing {
    /** FrameStart of the last completed frame, drop or not. ClientFrameStats_t+0x0. */
    uint32_t lastFrameTimestamp;
    /** Complete of the last displayed frame. ClientFrameStats_t+0x4. */
    uint32_t lastDisplayTimestamp;
    /** ClientFrameStats_t+0x8. The drain window chases this, not the newest received id. */
    uint16_t lastDisplayedFrameId;
    /** ClientFrameStats_t+0xa. Everything up to and including this id has been reported. */
    uint16_t sendCursor;
    IHS_VideoFrameStats slots[IHS_VIDEO_FRAME_STATS_RING_SIZE];
} IHS_VideoFrameStatsRing;

/**
 * What DataReceived knows about a freshly arrived data frame. Timestamps carried in the frame
 * header are in the *host's* clock; the ones the client takes are local. RecordFrameReceived
 * @ 0x1fab7c re-bases the host's onto ours when the host's clock is ahead, and so does this.
 */
typedef struct IHS_VideoFrameStatsInfo {
    uint16_t frameId;
    /** Data frame header timestamp: when the host started the frame. Event 5 (FrameStart). */
    uint32_t frameTimestamp;
    /** Packet header timestamp: when the host put it on the wire. Event 12 (Send). */
    uint32_t sendTimestamp;
    /** Local clock at arrival. Event 13 (Recv). */
    uint32_t recvTimestamp;
    /** Echoed back by the host; 0 means the frame reflects no input. */
    uint16_t inputMark;
    /** True when the mark was still in the input mark ring, i.e. the three fields below are set. */
    bool hasInputMark;
    /** When the input event happened, local clock. Event 0 (InputEventStart). */
    uint32_t inputEventTimestamp;
    /** When the input message was handed to the wire, local clock. Event 1 (InputEventSend). */
    uint32_t inputSendTimestamp;
    /** When the host received the input, host clock. Event 2 (InputEventRecv). */
    uint32_t inputRecvTimestamp;
} IHS_VideoFrameStatsInfo;

typedef struct IHS_FrameStatsAccumulator {
    struct {
        uint32_t count;
        double sum;
        double sumSquares;
    } values[IHS_FRAME_ACCUMULATED_STAT_COUNT];
} IHS_FrameStatsAccumulator;

void IHS_VideoFrameStatsRingInit(IHS_VideoFrameStatsRing *ring);

/**
 * Claim (or refresh) the ring slot for an arriving frame, mirroring RecordFrameReceived @ 0x1faac4.
 * The slot is claimed by whichever packet of the frame arrives first; a later packet of the same
 * frame only refreshes the Recv event, so Recv ends up being the *last* packet's arrival.
 *
 * A frame id 128 apart silently evicts the older record, exactly as the reference does.
 */
void IHS_VideoFrameStatsReceived(IHS_VideoFrameStatsRing *ring, const IHS_VideoFrameStatsInfo *info);

/**
 * Resolve a frame, mirroring RecordFrameComplete @ 0x1fafc8 including its two drop conditions: the
 * slot must still hold this frame id, and its result must still be Pending. (The reference's third
 * condition, "no connection", belongs to the caller.)
 *
 * @param now Local clock, recorded as event 18 (Complete).
 * @return true if the record was resolved, false if it was dropped.
 */
bool IHS_VideoFrameStatsComplete(IHS_VideoFrameStatsRing *ring, uint16_t frameId, EStreamFrameResult result,
                                 uint32_t now);

/**
 * Drain everything in `(sendCursor, lastDisplayedFrameId]` into the accumulator, mirroring the loop
 * in SendFrameEvents @ 0x1fb4f0: slots whose id no longer matches are stepped over, records still
 * Pending are forcibly resolved as DroppedLate, and every reported record is cleared afterwards.
 *
 * @return The number of frames drained. Nothing is sent when this is 0.
 */
size_t IHS_VideoFrameStatsDrain(IHS_VideoFrameStatsRing *ring, IHS_FrameStatsAccumulator *acc, uint32_t now);

/**
 * Number of ids in the half-open range `(previousFrameId, frameId)`, i.e. how many ids a
 * keyframe-driven reset skipped over. 16-bit arithmetic, so it wraps the way the ids do.
 */
uint16_t IHS_VideoFrameStatsGapSize(uint16_t previousFrameId, uint16_t frameId);

/**
 * Resolve every id a keyframe skipped over as k_EStreamFrameResultDroppedReset, mirroring the loop
 * at the head of CStreamDecoderVideo::DecodeFrame @ 0x205638. Neither `previousFrameId` nor
 * `frameId` is reported.
 *
 * Unlike the reference this refuses a gap wider than the ring. The reference walks the entire
 * 16-bit space when a keyframe id lands at or behind previousFrameId — up to 65535 events — and
 * gets away with it only because RecordFrameComplete @ 0x1fafc8 discards everything outside the
 * 128-entry ring, which is precisely what this loop feeds.
 *
 * @return The number of records actually resolved; 0 for an empty or over-wide gap.
 */
size_t IHS_VideoFrameStatsReportSkipped(IHS_VideoFrameStatsRing *ring, uint16_t previousFrameId, uint16_t frameId,
                                        uint32_t now);

void IHS_FrameStatsAccumulatorInit(IHS_FrameStatsAccumulator *acc);

/**
 * Fill `message->accumulated_stats` with every stat that got at least one sample, mirroring the
 * tail of SendFrameEvents @ 0x1fb754. Average and sample standard deviation follow
 * CStatValue::Calculate @ 0x257620: stddev divides by count-1 and is omitted when count < 2 or when
 * it computes to exactly 0.
 *
 * @return true if any value was written. Free with IHS_FrameStatsListClear.
 */
bool IHS_FrameStatsAccumulatorFill(const IHS_FrameStatsAccumulator *acc, CFrameStatsListMsg *message);

void IHS_FrameStatsListClear(CFrameStatsListMsg *message);
