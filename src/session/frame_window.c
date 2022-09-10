/*
 *  _____  _   _  _____  _  _  _     
 * |_   _|| | | |/  ___|| |(_)| |     Steam    
 *   | |  | |_| |\ `--. | | _ | |__     In-Home
 *   | |  |  _  | `--. \| || || '_ \      Streaming
 *  _| |_ | | | |/\__/ /| || || |_) |       Library
 *  \___/ \_| |_/\____/ |_||_||_.__/
 *
 * Copyright (c) 2022 Ningyuan Li <https://github.com/mariotaku>.
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

#include <memory.h>
#include <stdlib.h>
#include <assert.h>

#include "frame.h"
#include "crypto.h"
#include "session_pri.h"


struct IHS_SessionPacketsWindow {
    IHS_Mutex *mutex;
    IHS_SessionFramePacket *data;
    uint16_t capacity;
    /*
     * [+][+][+][-]
     *       ^ head.pos = 2
     */
    struct {
        int pos;
    } head;
    /*
     * [+][+][+][-]
     *         ^ tail.pos = 2
     */
    struct {
        int pos;
        uint16_t id;
    } tail;
};

static inline bool FrameItemIsHead(const IHS_SessionFramePacket *item);

static inline bool FrameItemIsUsed(const IHS_SessionFramePacket *item);

static inline void FrameItemCopy(IHS_SessionFramePacket *item, const IHS_SessionPacket *packet);

static inline void FrameItemRecycle(IHS_SessionFramePacket *item);

/**
 *
 * @param window
 */
IHS_SessionPacketsWindow *IHS_SessionPacketsWindowCreate(uint16_t capacity) {
    IHS_SessionPacketsWindow *window = calloc(1, sizeof(IHS_SessionPacketsWindow));
    window->mutex = IHS_MutexCreate();
    window->capacity = capacity;
    window->data = calloc(capacity, sizeof(IHS_SessionFramePacket));
    window->head.pos = 0;
    window->tail.pos = -1;
    return window;
}

void IHS_SessionPacketsWindowDestroy(IHS_SessionPacketsWindow *window) {
    IHS_MutexLock(window->mutex);
    for (int i = 0, j = window->capacity; i < j; i++) {
        if (!FrameItemIsUsed(&window->data[i])) {
            continue;
        }
        FrameItemRecycle(&window->data[i]);
    }
    free(window->data);
    IHS_MutexUnlock(window->mutex);
    IHS_MutexDestroy(window->mutex);
    free(window);
}

bool IHS_SessionPacketsWindowAdd(IHS_SessionPacketsWindow *window, const IHS_SessionPacket *packet) {
    IHS_MutexLock(window->mutex);
    /* Calculate distance of 2 packets */
    int tailOffset = window->tail.pos < 0 ? 1 : ((int) packet->header.packetId) - window->tail.id;
    bool ret = false;
    /* Packet with window->tail.id is guaranteed to be processed, so ignore it */
    /* Offset is over -32768, treat it as a rollover */
    if (tailOffset < INT16_MIN) {
        tailOffset = UINT16_MAX + tailOffset;
    }
    /* Offset is over 32767, also treat it as a rollover */
    if (tailOffset > INT16_MAX) {
        tailOffset = tailOffset - UINT16_MAX - 1;
    }
    /* We already processed this packet, so ignore it */
    if (tailOffset < 0 && -tailOffset > IHS_SessionPacketsWindowSize(window)) {
        ret = true;
        goto unlock;
    }
    /* Not sure why but the offset is significantly larger than window capacity. Ignore it first */
    if (tailOffset > window->capacity) {
        ret = true;
        goto unlock;
    }
    /* Large offset means overflow, abort processing and hangup */
    if (tailOffset > (int) IHS_SessionPacketsWindowAvailable(window)) {
        ret = false;
        goto unlock;
    }
    const int writePos = (window->tail.pos + tailOffset + window->capacity) % window->capacity;
    IHS_SessionFramePacket *tailPkt = &window->data[writePos];
    /* Ignore if the slot is used */
    if (FrameItemIsUsed(tailPkt)) {
        ret = true;
        goto unlock;
    }
    FrameItemCopy(tailPkt, packet);

    /* Only do incremental update */
    if (tailOffset > 0) {
        window->tail.pos = writePos;
        window->tail.id = packet->header.packetId;
    }
    ret = true;
    unlock:
    IHS_MutexUnlock(window->mutex);
    return ret;
}

bool IHS_SessionPacketsWindowPoll(IHS_SessionPacketsWindow *window, IHS_SessionFrame *frame) {
    IHS_MutexLock(window->mutex);
    uint16_t size = IHS_SessionPacketsWindowSize(window);
    bool ret = false;
    if (!size) {
        ret = false;
        goto unlock;
    }
    assert(window->head.pos >= 0);
    IHS_SessionFramePacket *head = &window->data[window->head.pos % window->capacity];

    /* Must start from packet head */
    if (!FrameItemIsHead(head)) {
        ret = false;
        goto unlock;
    }

    /* Must have size enough for all fragments */
    int packetsCount = 1 + head->header.fragmentId;
    if (size < packetsCount) {
        ret = false;
        goto unlock;
    }

    size_t frameBodyLen = 0;
    for (int i = window->head.pos, j = window->head.pos + packetsCount; i < j; i++) {
        /* The array is sparse, must collect all fragments */
        const IHS_SessionFramePacket *item = &window->data[i % window->capacity];
        if (!FrameItemIsUsed(item)) {
            ret = false;
            goto unlock;
        }
        frameBodyLen += item->bodyLen;
    }
    frame->header = head->header;
    IHS_BufferEnsureCapacity(&frame->body, frameBodyLen);

    for (int i = window->head.pos, j = window->head.pos + packetsCount; i < j; i++) {
        IHS_SessionFramePacket *item = &window->data[i % window->capacity];
        IHS_BufferAppend(&frame->body, item->body, item->bodyLen);

        /* This item is used, recycle it */
        FrameItemRecycle(item);
    }
    assert(frame->body.size == frameBodyLen);

    window->head.pos = window->head.pos + packetsCount;
    if (window->head.pos > window->capacity) {
        window->head.pos = window->head.pos % window->capacity;
    }
    ret = true;
    unlock:
    IHS_MutexUnlock(window->mutex);
    return ret;
}

uint16_t IHS_SessionPacketsWindowDiscard(IHS_SessionPacketsWindow *window, uint32_t diff) {
    IHS_MutexLock(window->mutex);
    uint16_t size = IHS_SessionPacketsWindowSize(window);
    uint16_t discarded = 0;
    if (!size) goto unlock;
    IHS_SessionFramePacket *tailPkt = &window->data[window->tail.pos];
    if (tailPkt->header.sendTimestamp < diff) goto unlock;
    /* Should discard all frames older than discardBefore */
    uint32_t discardBefore = tailPkt->header.sendTimestamp - diff;
    /* Find first valid index after discardBefore */
    int firstValid = -1;
    for (int i = window->head.pos, j = window->head.pos + size; i < j; i++) {
        const IHS_SessionFramePacket *item = &window->data[i % window->capacity];
        if (!FrameItemIsUsed(item) || !FrameItemIsHead(item)) {
            continue;
        }
        if (item->header.sendTimestamp >= discardBefore) {
            firstValid = i;
            break;
        }
    }
    if (firstValid < 0) goto unlock;
    for (int i = window->head.pos; i < firstValid; i++) {
        IHS_SessionFramePacket *item = &window->data[i % window->capacity];
        discarded++;
        if (!FrameItemIsUsed(item)) {
            continue;
        }
        /* This packet is used, recycle it */
        FrameItemRecycle(item);
    }
    window->head.pos = firstValid;
    if (window->head.pos > window->capacity) {
        window->head.pos = window->head.pos % window->capacity;
    }
    unlock:
    IHS_MutexUnlock(window->mutex);
    return discarded;
}

void IHS_SessionPacketsWindowReleaseFrame(IHS_SessionFrame *frame) {
    IHS_BufferClear(&frame->body, false);
}

uint16_t IHS_SessionPacketsWindowAvailable(const IHS_SessionPacketsWindow *window) {
    return window->capacity - IHS_SessionPacketsWindowSize(window);
}

uint16_t IHS_SessionPacketsWindowSize(const IHS_SessionPacketsWindow *window) {
    if (window->tail.pos < 0) {
        return 0;
    } else if (window->tail.pos + 1 >= window->head.pos) {
        /*
         * Head is before tail.pos, just calculate their distance
         *
         * |[+][+][+][+][+][-][-][-]| capacity = 8
         *  ^ head.pos = 0    ^ tail.pos = 4
         * distance = tail.pos + 1 - head.pos = 5
         *
         * |[-][-][-][-][-][-][-][-]| capacity = 8
         *                         ^ head.pos = 8, tail.pos = 7
         * distance = tail.pos + 1 - head.pos = 0
         */
        return window->tail.pos + 1 - window->head.pos;
    } else {
        /* Head is after tail.pos
         *
         * |[+][+][-][-][-][-][+][+]| capacity = 8
         *       ^ tail.pos = 1   ^ head.pos = 6
         * distance = capacity - head.pos + tail.pos + 1 = 4
         *
         * |[+][+][-][-][-][-][-][-]| capacity = 8
         *       ^ tail.pos = 1        ^ head.pos = 8
         * distance = capacity - head.pos + tail.pos + 1 = 2
         */
        return window->capacity - window->head.pos + window->tail.pos + 1;
    }
}

static inline bool FrameItemIsHead(const IHS_SessionFramePacket *item) {
    return item->header.type == IHS_SessionPacketTypeReliable || item->header.type == IHS_SessionPacketTypeUnreliable;
}

static inline bool FrameItemIsUsed(const IHS_SessionFramePacket *item) {
    return item->body != NULL;
}

static inline void FrameItemCopy(IHS_SessionFramePacket *item, const IHS_SessionPacket *packet) {
    item->header = packet->header;
    item->bodyLen = packet->bodyLen;
    item->body = malloc(packet->bodyLen);
    memcpy(item->body, packet->body, packet->bodyLen);
}

static inline void FrameItemRecycle(IHS_SessionFramePacket *item) {
    free(item->body);
    memset(item, 0, sizeof(IHS_SessionFramePacket));
}