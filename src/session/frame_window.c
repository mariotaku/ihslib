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
#include <malloc.h>
#include <stdlib.h>
#include <assert.h>

#include "frame.h"
#include "crypto.h"
#include "session_pri.h"

struct IHS_SessionPacketsWindow {
    IHS_SessionFramePacket *data;
    uint16_t capacity;
    /*
     * [+][+][+][-]
     *       ^ head = 2
     */
    int head;
    /*
     * [+][+][+][-]
     *         ^ tail = 2
     */
    int tail;
    uint16_t tailId;
};

/**
 *
 * @param window
 */
IHS_SessionPacketsWindow *IHS_SessionPacketsWindowCreate(uint16_t capacity) {
    IHS_SessionPacketsWindow *window = malloc(sizeof(IHS_SessionPacketsWindow));
    memset(window, 0, sizeof(IHS_SessionPacketsWindow));
    window->capacity = capacity;
    window->data = calloc(capacity, sizeof(IHS_SessionFramePacket));
    window->head = 0;
    window->tail = -1;
    return window;
}

void IHS_SessionPacketsWindowDestroy(IHS_SessionPacketsWindow *window) {
    free(window->data);
    free(window);
}

bool IHS_SessionPacketsWindowAdd(IHS_SessionPacketsWindow *window, const IHS_SessionPacket *packet) {
    /* Calculate distance of 2 packets */
    int tailOffset = window->tail < 0 ? 1 : (packet->header.packetId - window->tailId);
    /* Packet with window->tailId is guaranteed to be processed, so ignore it */
    if (tailOffset == 0) {
        return true;
    }
    /* Offset is over -32768, treat it as a rollover */
    if (tailOffset < INT16_MIN) {
        tailOffset = UINT16_MAX + tailOffset;
    }
    /* We already processed this packet, so ignore it */
    if (tailOffset < 0 && -tailOffset > IHS_SessionPacketsWindowSize(window)) {
        return true;
    }
    /* Large offset means overflow, abort processing and hangup */
    if (tailOffset > (int) IHS_SessionPacketsWindowAvailable(window)) {
        return false;
    }
    window->tail = (window->tail + tailOffset) % window->capacity;
    IHS_SessionFramePacket *fp = &window->data[window->tail];
    /* Ignore if the slot is used */
    if (fp->used) {
        return true;
    }
    fp->used = true;
    fp->header = packet->header;
    fp->bodyLen = packet->bodyLen;
    fp->body = malloc(packet->bodyLen);
    memcpy(fp->body, packet->body, packet->bodyLen);
    /* Only do incremental update */
    if (tailOffset > 0) {
        window->tailId = packet->header.packetId;
    }
    return true;
}

bool IHS_SessionPacketsWindowPoll(IHS_SessionPacketsWindow *window, IHS_SessionFrame *frame) {
    size_t size = IHS_SessionPacketsWindowSize(window);
    if (!size) return false;
    IHS_SessionFramePacket *data = window->data;
    assert(window->head >= 0);
    IHS_SessionFramePacket *packet = &data[window->head % window->capacity];
    IHS_SessionPacketType type = packet->header.type;
    /* Must start from packet head */
    if (type != IHS_SessionPacketTypeReliable && type != IHS_SessionPacketTypeUnreliable) {
        return false;
    }
    /* Must have size enough for all fragments */
    int packetsCount = 1 + packet->header.fragmentId;
    if (size < packetsCount) return false;
    size_t frameBodyLen = 0;
    for (int i = window->head, j = window->head + packetsCount; i < j; i++) {
        /* The array is sparse, must collect all fragments */
        const IHS_SessionFramePacket *item = &data[i % window->capacity];
        if (!item->used) return false;
        frameBodyLen += item->bodyLen;
    }
    uint8_t *frameBody = malloc(frameBodyLen);

    frame->header = data[window->head % window->capacity].header;
    frame->body = frameBody;
    frame->bodyLen = frameBodyLen;

    size_t frameBodyOffset = 0;
    for (int i = window->head, j = window->head + packetsCount; i < j; i++) {
        IHS_SessionFramePacket *item = &data[i % window->capacity];
        memcpy(&frameBody[frameBodyOffset], item->body, item->bodyLen);
        frameBodyOffset += item->bodyLen;

        /* This packet is used, recycle it */
        free(item->body);
        memset(item, 0, sizeof(IHS_SessionFramePacket));
    }

    window->head = window->head + packetsCount;
    if (window->head > window->capacity) {
        window->head = window->head % window->capacity;
    }
    return true;
}

void IHS_SessionPacketsWindowReleaseFrame(IHS_SessionFrame *frame) {
    free(frame->body);
}

uint16_t IHS_SessionPacketsWindowAvailable(const IHS_SessionPacketsWindow *window) {
    return window->capacity - IHS_SessionPacketsWindowSize(window);
}

uint16_t IHS_SessionPacketsWindowSize(const IHS_SessionPacketsWindow *window) {
    if (window->tail < 0) {
        return 0;
    } else if (window->tail + 1 >= window->head) {
        /*
         * Head is before tail, just calculate their distance
         *
         * |[+][+][+][+][+][-][-][-]| capacity = 8
         *  ^ head = 0    ^ tail = 4
         * distance = tail + 1 - head = 5
         *
         * |[-][-][-][-][-][-][-][-]| capacity = 8
         *                         ^ head = 8, tail = 7
         * distance = tail + 1 - head = 0
         */
        return window->tail + 1 - window->head;
    } else {
        /* Head is after tail
         *
         * |[+][+][-][-][-][-][+][+]| capacity = 8
         *       ^ tail = 1   ^ head = 6
         * distance = capacity - head + tail + 1 = 4
         *
         * |[+][+][-][-][-][-][-][-]| capacity = 8
         *       ^ tail = 1        ^ head = 8
         * distance = capacity - head + tail + 1 = 2
         */
        return window->capacity - window->head + window->tail + 1;
    }
}