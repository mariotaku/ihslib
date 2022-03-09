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

#include <stdint.h>

#include "session/channels/ch_data_video.h"

typedef struct IHS_SessionVideoPartialFrame {
    uint16_t sequence;
    uint8_t flags;
    uint16_t reserved1;
    uint16_t reserved2;
    uint8_t *data;
    size_t dataLen;
    struct IHS_SessionVideoPartialFrame *prev;
    struct IHS_SessionVideoPartialFrame *next;
} IHS_SessionVideoPartialFrame;

#define IHS_VideoPartialFrameForEach(head, name) for (IHS_SessionVideoPartialFrame *name = (head); name; name = name->next)

IHS_SessionVideoPartialFrame *IHS_VideoPartialFrameInsert(IHS_SessionVideoPartialFrame *head,
                                                          const IHS_SessionVideoFrameHeader *header,
                                                          const uint8_t *data, size_t dataLen);

IHS_SessionVideoPartialFrame *IHS_VideoPartialFrameClear(IHS_SessionVideoPartialFrame *head);