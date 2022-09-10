#pragma once

#include "ch_data_video.h"

#include <stddef.h>
#include <stdint.h>

typedef struct IHS_VideoPartialFrame {
    IHS_VideoFrameHeader header;
    IHS_Buffer data;
    struct IHS_VideoPartialFrame *prev;
    struct IHS_VideoPartialFrame *next;
} IHS_VideoPartialFrame;

typedef struct IHS_SessionVideoPartialFrames {
    IHS_VideoPartialFrame *head;
    IHS_VideoPartialFrame *tail;
} IHS_VideoPartialFrames;

void IHS_VideoPartialFramesInsertBefore(IHS_VideoPartialFrames *frames, IHS_VideoPartialFrame *before,
                                        const IHS_VideoFrameHeader *header, IHS_Buffer *data);

void IHS_VideoPartialFramesAppend(IHS_VideoPartialFrames *frames, const IHS_VideoFrameHeader *header,
                                  IHS_Buffer *data);

void IHS_VideoPartialFramesRemove(IHS_VideoPartialFrames *frames, IHS_VideoPartialFrame *node);

size_t IHS_VideoPartialFramesCount(const IHS_VideoPartialFrames *frames);

void IHS_VideoPartialFramesClear(IHS_VideoPartialFrames *frames);

#define IHS_VideoPartialFramesForEach(a, b) for((a) = (b)->head; (a) != NULL; (a) = (a)->next)