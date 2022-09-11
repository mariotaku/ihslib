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