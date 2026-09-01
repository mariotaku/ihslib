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

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdbool.h>

#include "frame_stats.h"
#include "protobuf/pb_utils.h"

/** The 0.001f floor SetLastFrameTimestamp @ 0x256638 and SetLastDisplayTimestamp @ 0x2566c4 apply. */
#define DELTA_FLOOR_MS 0.001f

/** StreamTimestampIsNewer: the wire clock is a free-running uint32, so compare as a signed delta. */
static inline bool TimestampIsNewer(uint32_t a, uint32_t b) {
    return (int32_t) (a - b) > 0;
}

/** GetStreamTimestampDeltaMS: 1/65536-second units to milliseconds. */
static inline float DeltaMS(uint32_t from, uint32_t to) {
    return (float) ((int32_t) (to - from)) * 1000.0f / 65536.0f;
}

static inline IHS_VideoFrameStats *SlotFor(IHS_VideoFrameStatsRing *ring, uint16_t frameId) {
    return &ring->slots[frameId & (IHS_VIDEO_FRAME_STATS_RING_SIZE - 1)];
}

static inline bool HasEvent(const IHS_VideoFrameStats *stats, EStreamFrameEvent event) {
    return (stats->eventMask & (1u << (unsigned) event)) != 0;
}

static inline void SetEvent(IHS_VideoFrameStats *stats, EStreamFrameEvent event, uint32_t timestamp) {
    stats->events[event] = timestamp;
    stats->eventMask |= 1u << (unsigned) event;
}

/**
 * A zeroed slot has frameId 0, which would otherwise make frame 0 look like a claimed record. The
 * reference has the same corner and lives with it; the event mask is a free way to tell a claimed
 * slot from a never-written one, so use it everywhere the reference compares only the id.
 */
static inline bool IsClaimed(const IHS_VideoFrameStats *stats, uint16_t frameId) {
    return stats->eventMask != 0 && stats->frameId == frameId;
}

static inline bool IsDisplayed(const IHS_VideoFrameStats *stats) {
    return stats->result == k_EStreamFrameResultDisplayed;
}

/** HasRoundTripLatency @ 0x209db4. */
static inline bool HasRoundTripLatency(const IHS_VideoFrameStats *stats) {
    return HasEvent(stats, k_EStreamInputEventStart) && IsDisplayed(stats);
}

static void AddSample(IHS_FrameStatsAccumulator *acc, EFrameAccumulatedStat stat, float value) {
    if (acc == NULL) {
        return;
    }
    acc->values[stat].count += 1;
    acc->values[stat].sum += value;
    acc->values[stat].sumSquares += (double) value * (double) value;
}

/**
 * CFrameStatsAccumulator::AddFrameSample @ 0x256f00 with ESampleSource = 2 (client). Only the
 * client half runs: stats 1..5 need the host's capture/encode events, and 7 (Decode) and 8
 * (Display) need events 14..17, which live inside the application's decoder and never reach here.
 */
static void AddFrameSample(IHS_FrameStatsAccumulator *acc, const IHS_VideoFrameStats *stats) {
    if (HasEvent(stats, k_EStreamFrameEventRecv) && HasEvent(stats, k_EStreamFrameEventSend)) {
        AddSample(acc, k_EFrameStatNetworkDurationMS,
                  DeltaMS(stats->events[k_EStreamFrameEventSend], stats->events[k_EStreamFrameEventRecv]));
    }
    if (IsDisplayed(stats)) {
        uint32_t complete = stats->events[k_EStreamFrameEventComplete];
        if (HasEvent(stats, k_EStreamFrameEventRecv)) {
            AddSample(acc, k_EFrameStatClientDurationMS,
                      DeltaMS(stats->events[k_EStreamFrameEventRecv], complete));
        }
        if (HasEvent(stats, k_EStreamFrameEventStart)) {
            AddSample(acc, k_EFrameStatFrameDurationMS,
                      DeltaMS(stats->events[k_EStreamFrameEventStart], complete));
        }
        // GetFrameFPS @ 0x254c38 and GetDisplayFPS @ 0x254ca0 are both 1000/delta, 0 when the delta
        // is 0. The display rate wins unless the two disagree by more than 100 fps, in which case
        // the smaller is taken (`10000.0 < diff * diff` @ 0x256f00). A frame with neither delta —
        // the first one of a session — contributes a 0 sample, exactly as the reference does.
        float frameFPS = stats->frameStartDelta != 0.0f ? 1000.0f / stats->frameStartDelta : 0.0f;
        float displayFPS = stats->frameDisplayDelta != 0.0f ? 1000.0f / stats->frameDisplayDelta : 0.0f;
        float fps = displayFPS;
        float difference = frameFPS - displayFPS;
        if (difference * difference > 10000.0f) {
            fps = frameFPS < displayFPS ? frameFPS : displayFPS;
        }
        AddSample(acc, k_EFrameStatFPS, fps);
    }
    if (HasRoundTripLatency(stats)) {
        uint32_t inputStart = stats->events[k_EStreamInputEventStart];
        uint32_t inputRecv = stats->events[k_EStreamInputEventRecv];
        // GetInputLatency @ 0x2562e0 yields 0 rather than a negative span.
        AddSample(acc, k_EFrameStatInputLatencyMS,
                  TimestampIsNewer(inputRecv, inputStart) ? DeltaMS(inputStart, inputRecv) : 0.0f);
        AddSample(acc, k_EFrameStatGameLatencyMS,
                  DeltaMS(inputRecv, stats->events[k_EStreamFrameEventStart]));
        AddSample(acc, k_EFrameStatRoundTripLatencyMS,
                  DeltaMS(inputStart, stats->events[k_EStreamFrameEventComplete]));
    }
}

void IHS_VideoFrameStatsRingInit(IHS_VideoFrameStatsRing *ring) {
    memset(ring, 0, sizeof(*ring));
}

void IHS_VideoFrameStatsReceived(IHS_VideoFrameStatsRing *ring, const IHS_VideoFrameStatsInfo *info) {
    IHS_VideoFrameStats *stats = SlotFor(ring, info->frameId);
    if (IsClaimed(stats, info->frameId)) {
        // A later packet of a frame already being assembled only moves Recv forward, so Recv ends
        // up marking the *last* packet of the frame.
        SetEvent(stats, k_EStreamFrameEventRecv, info->recvTimestamp);
        return;
    }

    uint32_t frameTimestamp = info->frameTimestamp;
    uint32_t sendTimestamp = info->sendTimestamp;
    uint32_t inputRecvTimestamp = info->inputRecvTimestamp;
    // 0x1fab7c: when the host's clock reads ahead of ours the host-side timestamps are shifted back
    // onto our clock, so that the spans mixing the two clocks below cannot come out negative.
    if (TimestampIsNewer(sendTimestamp, info->recvTimestamp)) {
        uint32_t skew = sendTimestamp - info->recvTimestamp;
        sendTimestamp -= skew;
        frameTimestamp -= skew;
        if (inputRecvTimestamp != 0) {
            inputRecvTimestamp -= skew;
        }
    }

    memset(stats, 0, sizeof(*stats));
    stats->frameId = info->frameId;
    SetEvent(stats, k_EStreamFrameEventStart, frameTimestamp);
    SetEvent(stats, k_EStreamFrameEventSend, sendTimestamp);
    SetEvent(stats, k_EStreamFrameEventRecv, info->recvTimestamp);
    // FinishInputMark @ 0x1fb364 writes the input trio only for a mark still in the ring.
    if (info->inputMark != 0 && info->hasInputMark) {
        stats->inputMark = info->inputMark;
        SetEvent(stats, k_EStreamInputEventStart, info->inputEventTimestamp);
        SetEvent(stats, k_EStreamInputEventSend, info->inputSendTimestamp);
        SetEvent(stats, k_EStreamInputEventRecv, inputRecvTimestamp);
    }
}

bool IHS_VideoFrameStatsComplete(IHS_VideoFrameStatsRing *ring, uint16_t frameId, EStreamFrameResult result,
                                 uint32_t now) {
    IHS_VideoFrameStats *stats = SlotFor(ring, frameId);
    if (!IsClaimed(stats, frameId)) {
        return false;
    }
    if (stats->result != k_EStreamFrameResultPending) {
        return false;
    }
    SetEvent(stats, k_EStreamFrameEventComplete, now);
    // The frame chain advances for every completed frame, drops included; the display chain only
    // for frames that made it to the screen.
    if (ring->lastFrameTimestamp != 0 && HasEvent(stats, k_EStreamFrameEventStart)) {
        stats->frameStartDelta = DeltaMS(ring->lastFrameTimestamp, stats->events[k_EStreamFrameEventStart]);
        if (stats->frameStartDelta <= 0.0f) {
            stats->frameStartDelta = DELTA_FLOOR_MS;
        }
    }
    if (HasEvent(stats, k_EStreamFrameEventStart) && stats->events[k_EStreamFrameEventStart] != 0) {
        ring->lastFrameTimestamp = stats->events[k_EStreamFrameEventStart];
    }
    if (ring->lastDisplayTimestamp != 0) {
        stats->frameDisplayDelta = DeltaMS(ring->lastDisplayTimestamp, now);
        if (stats->frameDisplayDelta <= 0.0f) {
            stats->frameDisplayDelta = DELTA_FLOOR_MS;
        }
    }
    stats->result = result;
    if (IsDisplayed(stats)) {
        ring->lastDisplayedFrameId = frameId;
        ring->lastDisplayTimestamp = now;
    }
    return true;
}

size_t IHS_VideoFrameStatsDrain(IHS_VideoFrameStatsRing *ring, IHS_FrameStatsAccumulator *acc, uint32_t now) {
    size_t drained = 0;
    // The window chases the last *displayed* frame, so frames after it stay pending for a later
    // tick. The cursor steps past evicted or never-claimed ids without reporting them.
    while (ring->sendCursor != ring->lastDisplayedFrameId) {
        uint16_t frameId = ring->sendCursor + 1;
        IHS_VideoFrameStats *stats = SlotFor(ring, frameId);
        if (IsClaimed(stats, frameId)) {
            if (stats->result == k_EStreamFrameResultPending) {
                // Its turn to be reported came and nothing ever resolved it (0x1fb5f4).
                IHS_VideoFrameStatsComplete(ring, frameId, k_EStreamFrameResultDroppedLate, now);
            }
            AddFrameSample(acc, stats);
            drained += 1;
            memset(stats, 0, sizeof(*stats));
        }
        ring->sendCursor = frameId;
    }
    return drained;
}

uint16_t IHS_VideoFrameStatsGapSize(uint16_t previousFrameId, uint16_t frameId) {
    return (uint16_t) (frameId - previousFrameId - 1);
}

size_t IHS_VideoFrameStatsReportSkipped(IHS_VideoFrameStatsRing *ring, uint16_t previousFrameId, uint16_t frameId,
                                        uint32_t now) {
    uint16_t gap = IHS_VideoFrameStatsGapSize(previousFrameId, frameId);
    if (gap == 0 || gap > IHS_VIDEO_FRAME_STATS_RING_SIZE) {
        return 0;
    }
    size_t reported = 0;
    for (uint16_t i = 1; i <= gap; i++) {
        if (IHS_VideoFrameStatsComplete(ring, (uint16_t) (previousFrameId + i), k_EStreamFrameResultDroppedReset,
                                        now)) {
            reported += 1;
        }
    }
    return reported;
}

void IHS_FrameStatsAccumulatorInit(IHS_FrameStatsAccumulator *acc) {
    memset(acc, 0, sizeof(*acc));
}

bool IHS_FrameStatsAccumulatorFill(const IHS_FrameStatsAccumulator *acc, CFrameStatsListMsg *message) {
    size_t count = 0;
    for (size_t i = 0; i < IHS_FRAME_ACCUMULATED_STAT_COUNT; i++) {
        if (acc->values[i].count > 0) {
            count += 1;
        }
    }
    if (count == 0) {
        return false;
    }
    CFrameStatAccumulatedValue **values = calloc(count, sizeof(CFrameStatAccumulatedValue *));
    size_t written = 0;
    for (size_t i = 0; i < IHS_FRAME_ACCUMULATED_STAT_COUNT; i++) {
        uint32_t samples = acc->values[i].count;
        if (samples == 0) {
            continue;
        }
        CFrameStatAccumulatedValue *value = calloc(1, sizeof(CFrameStatAccumulatedValue));
        cframe_stat_accumulated_value__init(value);
        value->stat_type = (EFrameAccumulatedStat) i;
        value->count = (int32_t) samples;
        // CStatValue::Calculate @ 0x257620: a lone sample is reported as-is with no deviation.
        if (samples < 2) {
            value->average = (float) acc->values[i].sum;
        } else {
            double average = acc->values[i].sum / samples;
            // Σ(x - avg)² expanded, so the samples themselves needn't be kept. The reference
            // divides by count-1, i.e. the sample standard deviation.
            double variance = (acc->values[i].sumSquares - average * acc->values[i].sum) / (samples - 1);
            value->average = (float) average;
            float stddev = variance > 0 ? (float) sqrt(variance) : 0.0f;
            if (stddev != 0.0f) {
                PROTOBUF_C_P_SET_VALUE(value, stddev, stddev);
            }
        }
        values[written++] = value;
    }
    message->accumulated_stats = values;
    message->n_accumulated_stats = written;
    return true;
}

void IHS_FrameStatsListClear(CFrameStatsListMsg *message) {
    for (size_t i = 0; i < message->n_accumulated_stats; i++) {
        free(message->accumulated_stats[i]);
    }
    free(message->accumulated_stats);
    message->accumulated_stats = NULL;
    message->n_accumulated_stats = 0;
}
