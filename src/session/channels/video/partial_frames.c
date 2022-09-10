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

void IHS_VideoPartialFramesClear(IHS_VideoPartialFrames *frames) {
    for (IHS_VideoPartialFrame *cur = frames->head; cur != NULL;) {
        IHS_VideoPartialFrame *next = cur->next;
        FreeNode(cur);
        cur = next;
    }
    frames->head = NULL;
    frames->tail = NULL;
}

static IHS_VideoPartialFrame *NewNode(const IHS_VideoFrameHeader *header, IHS_Buffer *data) {
    assert(header != NULL);
    assert(data != NULL && data->data != NULL);
    IHS_VideoPartialFrame *node = calloc(1, sizeof(IHS_VideoPartialFrame));
    node->header = *header;
    IHS_BufferTakeOwnership(&node->data, data);
    return node;
}

static void FreeNode(IHS_VideoPartialFrame *node) {
    assert(node != NULL);
    IHS_BufferClear(&node->data, true);
    free(node);
}