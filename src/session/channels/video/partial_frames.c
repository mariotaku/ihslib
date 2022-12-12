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

#include "partial_frames.h"

#include <stdlib.h>

static IHS_VideoPartialFrame *NewNode(const IHS_VideoFrameHeader *header, IHS_Buffer *data);

static void FreeNode(IHS_VideoPartialFrame *node);

void IHS_VideoPartialFramesInsertBefore(IHS_VideoPartialFrames *frames, IHS_VideoPartialFrame *before,
                                        const IHS_VideoFrameHeader *header, IHS_Buffer *data) {
    assert (frames != NULL);
    assert (before != NULL);
    IHS_VideoPartialFrame *inserted = NewNode(header, data);

    IHS_VideoPartialFrame *prev = before->prev;
    if (prev == NULL) {
        // Inserting to the start of the linked list
        assert(frames->head == before);
        frames->head = inserted;
    } else {
        prev->next = inserted;
    }
    before->prev = inserted;

    inserted->prev = prev;
    inserted->next = before;
}

void IHS_VideoPartialFramesAppend(IHS_VideoPartialFrames *frames, const IHS_VideoFrameHeader *header,
                                  IHS_Buffer *data) {
    assert (frames != NULL);
    IHS_VideoPartialFrame *inserted = NewNode(header, data);

    IHS_VideoPartialFrame *oldHead = frames->head, *oldTail = frames->tail;
    if (oldHead == NULL) {
        // The frames must be empty
        assert(oldTail == NULL);
        frames->head = inserted;
    }
    if (oldTail != NULL) {
        oldTail->next = inserted;
        inserted->prev = oldTail;
    }
    frames->tail = inserted;
}

void IHS_VideoPartialFramesRemove(IHS_VideoPartialFrames *frames, IHS_VideoPartialFrame *node) {
    IHS_VideoPartialFrame *prev = node->prev;
    IHS_VideoPartialFrame *next = node->next;
    if (prev != NULL) {
        prev->next = next;
    } else {
        frames->head = next;
    }
    if (next != NULL) {
        next->prev = prev;
    } else {
        frames->tail = prev;
    }
    free(node);
}

size_t IHS_VideoPartialFramesCount(const IHS_VideoPartialFrames *frames) {
    size_t count = 0;
    IHS_VideoPartialFrame *cur;
    IHS_VideoPartialFramesForEach(cur, frames) {
        count++;
    }
    return count;
}

size_t IHS_VideoPartialFramesClear(IHS_VideoPartialFrames *frames) {
    size_t count = 0;
    for (IHS_VideoPartialFrame *cur = frames->head; cur != NULL;) {
        IHS_VideoPartialFrame *next = cur->next;
        FreeNode(cur);
        cur = next;
        count += 1;
    }
    frames->head = NULL;
    frames->tail = NULL;
    return count;
}

static IHS_VideoPartialFrame *NewNode(const IHS_VideoFrameHeader *header, IHS_Buffer *data) {
    assert(header != NULL);
    assert(data != NULL && data->data != NULL);
    IHS_VideoPartialFrame *node = calloc(1, sizeof(IHS_VideoPartialFrame));
    node->header = *header;
    IHS_BufferTransferOwnership(data, &node->data);
    return node;
}

static void FreeNode(IHS_VideoPartialFrame *node) {
    assert(node != NULL);
    IHS_BufferClear(&node->data, true);
    free(node);
}