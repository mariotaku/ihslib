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

#include <malloc.h>
#include <string.h>

#include "partial_frame.h"

typedef IHS_SessionVideoPartialFrame PF;

IHS_SessionVideoPartialFrame *IHS_VideoPartialFrameInsert(IHS_SessionVideoPartialFrame *head,
                                                          const IHS_SessionVideoFrameHeader *header,
                                                          const uint8_t *data, size_t dataLen) {
    IHS_SessionVideoPartialFrame *cur = malloc(sizeof(IHS_SessionVideoPartialFrame));
    memset(cur, 0, sizeof(IHS_SessionVideoPartialFrame));

    cur->sequence = header->sequence;
    cur->flags = header->flags;
    cur->reserved1 = header->reserved1;
    cur->reserved2 = header->reserved2;
    cur->data = malloc(dataLen);
    cur->dataLen = dataLen;
    memcpy(cur->data, data, dataLen);

    if (!head) return cur;
    PF *tail;
    for (tail = head; tail && tail->next; tail = tail->next) {
        if (tail->sequence == cur->sequence && cur->reserved2 < tail->reserved1) break;
    }
    tail->next = cur;
    cur->prev = tail;
    return head;
}
IHS_SessionVideoPartialFrame *IHS_VideoPartialFrameClear(IHS_SessionVideoPartialFrame *head) {
    for (IHS_SessionVideoPartialFrame *cur = head; cur;) {
        IHS_SessionVideoPartialFrame *tmp = cur;
        cur = cur->next;
        free(tmp->data);
        free(tmp);
    }
    return NULL;
}