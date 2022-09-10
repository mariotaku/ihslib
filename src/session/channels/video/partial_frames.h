#pragma once

#include "ch_data_video.h"

#include <stddef.h>
#include <stdint.h>

typedef struct IHS_SessionVideoPartialFrame {
    IHS_VideoFrameHeader header;
    IHS_Buffer data;
    struct IHS_SessionVideoPartialFrame *prev;
    struct IHS_SessionVideoPartialFrame *next;
} IHS_SessionVideoPartialFrame;

typedef struct IHS_SessionVideoPartialFrames {
    IHS_SessionVideoPartialFrame *head;
    IHS_SessionVideoPartialFrame *tail;
} IHS_SessionVideoPartialFrames;

IHS_SessionVideoPartialFrame *IHS_SessionVideoPartialFramesInsertBefore(IHS_SessionVideoPartialFrames *frames,
                                                                        IHS_SessionVideoPartialFrame *node);

IHS_SessionVideoPartialFrame *IHS_SessionVideoPartialFramesAppend(IHS_SessionVideoPartialFrames *frames);

void IHS_SessionVideoPartialFramesRemove(IHS_SessionVideoPartialFrames *frames, IHS_SessionVideoPartialFrame *node);

#define IHS_SessionVideoPartialFramesForEach(a, b) for((a) = (b)->head; (a) != NULL; (a) = (a)->next)