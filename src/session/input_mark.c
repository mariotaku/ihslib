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

#include <string.h>

#include "input_mark.h"
#include "packet.h"

/** GetInputMarkIndex @ 0x1f4e30. */
static inline size_t MarkIndex(uint16_t mark) {
    return mark & (IHS_INPUT_MARK_RING_SIZE - 1);
}

void IHS_SessionInputMarksInit(IHS_SessionInputMarks *marks) {
    memset(marks, 0, sizeof(*marks));
    marks->lock = IHS_MutexCreate();
}

void IHS_SessionInputMarksDeinit(IHS_SessionInputMarks *marks) {
    IHS_MutexDestroy(marks->lock);
    marks->lock = NULL;
}

uint16_t IHS_SessionInputMarkNext(IHS_SessionInputMarks *marks, uint32_t eventTimestamp) {
    IHS_MutexLock(marks->lock);
    // Skip 0 on wrap, exactly as the reference's do/while does: 0 is the "no mark" sentinel the
    // host puts in a frame header when the frame reflects no input.
    do {
        marks->counter += 1;
    } while (marks->counter == 0);
    uint16_t mark = marks->counter;
    IHS_SessionInputMarkEntry *entry = &marks->ring[MarkIndex(mark)];
    entry->mark = mark;
    entry->eventTimestamp = eventTimestamp;
    entry->sendTimestamp = IHS_SessionPacketTimestamp();
    IHS_MutexUnlock(marks->lock);
    return mark;
}

bool IHS_SessionInputMarkFinish(IHS_SessionInputMarks *marks, uint16_t mark, uint32_t hostRecvTimestamp,
                                uint32_t now, IHS_SessionInputMarkEntry *out) {
    if (mark == 0) {
        return false;
    }
    IHS_MutexLock(marks->lock);
    const IHS_SessionInputMarkEntry *entry = &marks->ring[MarkIndex(mark)];
    // The 16-bit counter wraps onto a 1024-slot ring, so a slot is reused every 1024 marks. A
    // mismatch means this mark was evicted before the frame reflecting it came back.
    if (entry->mark != mark) {
        IHS_MutexUnlock(marks->lock);
        return false;
    }
    marks->latest.inputMark = mark;
    marks->latest.queuedToSent = entry->sendTimestamp - entry->eventTimestamp;
    marks->latest.roundTrip = now - entry->eventTimestamp;
    marks->latest.hostRecvTimestamp = hostRecvTimestamp;
    marks->hasLatest = true;
    if (out != NULL) {
        *out = *entry;
    }
    IHS_MutexUnlock(marks->lock);
    return true;
}

bool IHS_SessionInputMarksGetLatest(IHS_SessionInputMarks *marks, IHS_SessionInputLatency *out) {
    IHS_MutexLock(marks->lock);
    bool has = marks->hasLatest;
    if (has) {
        *out = marks->latest;
    }
    IHS_MutexUnlock(marks->lock);
    return has;
}
