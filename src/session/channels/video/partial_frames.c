#include "partial_frames.h"

#include <stdlib.h>

IHS_SessionVideoPartialFrame *IHS_SessionVideoPartialFramesInsertBefore(IHS_SessionVideoPartialFrames *frames,
                                                                        IHS_SessionVideoPartialFrame *node) {
    assert (frames != NULL);
    assert (node != NULL);
    IHS_SessionVideoPartialFrame *inserted = calloc(1, sizeof(IHS_SessionVideoPartialFrame));
    IHS_SessionVideoPartialFrame *prev = node->prev;
    if (prev == NULL) {
        assert(frames->head == node);
        frames->head = inserted;
    } else {
        prev->next = inserted;
    }
    node->prev = inserted;

    inserted->prev = prev;
    inserted->next = node;
    return inserted;
}

IHS_SessionVideoPartialFrame *IHS_SessionVideoPartialFramesAppend(IHS_SessionVideoPartialFrames *frames) {
    assert (frames != NULL);
    IHS_SessionVideoPartialFrame *inserted = calloc(1, sizeof(IHS_SessionVideoPartialFrame));
    IHS_SessionVideoPartialFrame *oldHead = frames->head, *oldTail = frames->tail;
    if (oldHead == NULL) {
        frames->head = inserted;
    } else {
        oldHead->next = inserted;
    }
    if (oldTail != NULL) {
        oldTail->next = inserted;
        inserted->prev = oldTail;
    }
    frames->tail = inserted;
    return inserted;
}

void IHS_SessionVideoPartialFramesRemove(IHS_SessionVideoPartialFrames *frames, IHS_SessionVideoPartialFrame *node) {
    IHS_SessionVideoPartialFrame *prev = node->prev;
    IHS_SessionVideoPartialFrame *next = node->next;
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