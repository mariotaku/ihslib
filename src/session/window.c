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
#include "window.h"

#include <memory.h>
#include <stdlib.h>
#include <assert.h>

struct IHS_SessionPacketsWindow {
    IHS_SessionWindowItem *data;
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

static inline bool FrameItemIsHead(const IHS_SessionWindowItem *item);

static inline bool FrameItemIsUsed(const IHS_SessionWindowItem *item);

static inline void FrameItemUsePacket(IHS_SessionWindowItem *item, IHS_SessionPacket *packet);

static inline void FrameItemRecycle(IHS_SessionWindowItem *item);

IHS_SessionPacketsWindow *IHS_SessionPacketsWindowCreate(uint16_t capacity) {
    IHS_SessionPacketsWindow *window = calloc(1, sizeof(IHS_SessionPacketsWindow));
    window->capacity = capacity;
    window->data = calloc(capacity, sizeof(IHS_SessionWindowItem));
    window->head.pos = 0;
    window->tail.pos = -1;
    return window;
}

void IHS_SessionPacketsWindowDestroy(IHS_SessionPacketsWindow *window) {
    for (int i = 0, j = window->capacity; i < j; i++) {
        if (!FrameItemIsUsed(&window->data[i])) {
            continue;
        }
        FrameItemRecycle(&window->data[i]);
    }
    free(window->data);
    free(window);
}

bool IHS_SessionPacketsWindowAdd(IHS_SessionPacketsWindow *window, IHS_SessionPacket *packet) {
    /* Calculate distance of 2 items */
    int tailOffset = window->tail.pos < 0 ? 1 : (int16_t) (packet->header.packetId - window->tail.id);
    /* We already processed this packet, so ignore it */
    if (tailOffset < 0 && -tailOffset > IHS_SessionPacketsWindowSize(window)) {
        return true;
    }
    /* Not sure why but the offset is significantly larger than window capacity. Ignore it reset */
    if (tailOffset > window->capacity) {
        return true;
    }
    /* Large offset means overflow, abort processing and hangup */
    if (tailOffset > (int) IHS_SessionPacketsWindowAvailable(window)) {
        return false;
    }
    const int writePos = (window->tail.pos + tailOffset + window->capacity) % window->capacity;
    IHS_SessionWindowItem *tailPkt = &window->data[writePos];
    /* Ignore if the slot is used */
    if (FrameItemIsUsed(tailPkt)) {
        return true;
    }
    FrameItemUsePacket(tailPkt, packet);

    /* Only do incremental update */
    if (tailOffset > 0) {
        window->tail.pos = writePos;
        window->tail.id = packet->header.packetId;
    }
    return true;
}

bool IHS_SessionPacketsWindowPoll(IHS_SessionPacketsWindow *window, IHS_SessionFrame *frame) {
    uint16_t size = IHS_SessionPacketsWindowSize(window);
    if (size == 0) {
        return false;
    }
    assert(window->head.pos >= 0);
    IHS_SessionWindowItem *head = &window->data[window->head.pos % window->capacity];

    /* Must start from packet head */
    if (!FrameItemIsHead(head)) {
        return false;
    }

    /* Must have size enough for all fragments */
    int packetsCount = 1 + head->header.fragmentId;
    if (size < packetsCount) {
        return false;
    }

    size_t frameBodyLen = 0;
    for (int i = window->head.pos, j = window->head.pos + packetsCount; i < j; i++) {
        /* The array is sparse, must collect all fragments */
        const IHS_SessionWindowItem *item = &window->data[i % window->capacity];
        if (!FrameItemIsUsed(item)) {
            return false;
        }
        frameBodyLen += item->body.size;
    }
    frame->header = head->header;
    IHS_BufferEnsureMaxSize(&frame->body, frameBodyLen);

    for (int i = window->head.pos, j = window->head.pos + packetsCount; i < j; i++) {
        IHS_SessionWindowItem *item = &window->data[i % window->capacity];
        IHS_BufferAppend(&frame->body, &item->body);

        /* This item is used, recycle it */
        FrameItemRecycle(item);
    }
    assert(frame->body.size == frameBodyLen);

    window->head.pos = window->head.pos + packetsCount;
    if (window->head.pos > window->capacity) {
        window->head.pos = window->head.pos % window->capacity;
    }
    return true;
}

uint16_t IHS_SessionPacketsWindowDiscard(IHS_SessionPacketsWindow *window, uint32_t diff) {
    uint16_t size = IHS_SessionPacketsWindowSize(window);
    if (!size) return 0;
    IHS_SessionWindowItem *tailPkt = &window->data[window->tail.pos];
    if (tailPkt->header.sendTimestamp < diff) return 0;
    /* Should discard all frames older than discardBefore */
    uint32_t discardBefore = tailPkt->header.sendTimestamp - diff;
    /* Find reset valid index after discardBefore */
    int firstValid = -1;
    for (int i = window->head.pos, j = window->head.pos + size; i < j; i++) {
        const IHS_SessionWindowItem *item = &window->data[i % window->capacity];
        if (!FrameItemIsUsed(item) || !FrameItemIsHead(item)) {
            continue;
        }
        if (item->header.sendTimestamp >= discardBefore) {
            firstValid = i;
            break;
        }
    }
    if (firstValid < 0) return 0;
    uint16_t discarded = 0;
    for (int i = window->head.pos; i < firstValid; i++) {
        IHS_SessionWindowItem *item = &window->data[i % window->capacity];
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
    return discarded;
}

void IHS_SessionPacketsWindowReleaseFrame(IHS_SessionFrame *frame) {
    IHS_SessionFrameClear(frame, false);
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

static inline bool FrameItemIsHead(const IHS_SessionWindowItem *item) {
    return item->header.type == IHS_SessionPacketTypeReliable || item->header.type == IHS_SessionPacketTypeUnreliable;
}

static inline bool FrameItemIsUsed(const IHS_SessionWindowItem *item) {
    return !IHS_BufferIsNull(&item->body);
}

static inline void FrameItemUsePacket(IHS_SessionWindowItem *item, IHS_SessionPacket *packet) {
    item->header = packet->header;
    IHS_BufferTransferOwnership(&packet->body, &item->body);
}

static inline void FrameItemRecycle(IHS_SessionWindowItem *item) {
    IHS_BufferClear(&item->body, true);
    memset(item, 0, sizeof(IHS_SessionWindowItem));
}