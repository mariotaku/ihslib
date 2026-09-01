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

#pragma once

#include <stdint.h>
#include <stdbool.h>

#include "ihslib/input.h"
#include "ihs_thread.h"

/**
 * Ring capacity. Steam's GetInputMarkIndex @ 0x1f4e30 is `mark & 0x3ff`, so the ring is 1024 wide
 * and the 16-bit counter wraps onto it 64 times before repeating.
 */
#define IHS_INPUT_MARK_RING_SIZE 1024

typedef struct IHS_SessionInputMarkEntry {
    /** 0 means the slot was never written. Marks themselves never take the value 0. */
    uint16_t mark;
    uint32_t eventTimestamp;
    uint32_t sendTimestamp;
} IHS_SessionInputMarkEntry;

typedef struct IHS_SessionInputMarks {
    IHS_Mutex *lock;
    /**
     * Pre-incremented per input message and never allowed to be 0 — CreateInputMark @ 0x1fb264
     * loops until it is non-zero, so the host can read 0 as "this message carries no mark".
     */
    uint16_t counter;
    IHS_SessionInputMarkEntry ring[IHS_INPUT_MARK_RING_SIZE];
    IHS_SessionInputLatency latest;
    bool hasLatest;
} IHS_SessionInputMarks;

typedef struct IHS_Session IHS_Session;

void IHS_SessionInputMarksInit(IHS_SessionInputMarks *marks);

void IHS_SessionInputMarksDeinit(IHS_SessionInputMarks *marks);

/**
 * Take the next mark and record when the input happened, mirroring CreateInputMark @ 0x1fb264.
 *
 * @param eventTimestamp When the input actually occurred, in IHS_SessionPacketTimestamp() units.
 *                       Pass IHS_SessionPacketTimestamp() if the caller has nothing better; the
 *                       reference does exactly that in SetMouseCentered @ 0x2263b0.
 * @return The mark to put in the message's input_mark field. Never 0.
 */
uint16_t IHS_SessionInputMarkNext(IHS_SessionInputMarks *marks, uint32_t eventTimestamp);

/**
 * Match a mark echoed back in a video frame header against the ring and compute the latency.
 * Mirrors FinishInputMark @ 0x1fb364, except that it indexes by the mark it was given rather than
 * by the current counter — the reference uses `GetInputMarkIndex(this, this->inputMark)` there,
 * which only ever resolves the newest mark and looks like an upstream bug.
 *
 * @param mark The inputMark from the frame header. 0 means the frame reflects no input.
 * @param hostRecvTimestamp The frame header's inputRecvTimestamp, in the *host's* clock.
 * @return false if the mark is 0 or has already been evicted from the ring.
 */
bool IHS_SessionInputMarkFinish(IHS_SessionInputMarks *marks, uint16_t mark, uint32_t hostRecvTimestamp,
                                uint32_t now);

bool IHS_SessionInputMarksGetLatest(IHS_SessionInputMarks *marks, IHS_SessionInputLatency *out);
