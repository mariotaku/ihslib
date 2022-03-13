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

#include <stdlib.h>
#include <assert.h>
#include "ihs_queue.h"
#include "ihs_thread.h"

typedef struct QueueNode {
    struct QueueNode *next;
} QueueNode;

struct IHS_Queue {
    size_t itemSize;
    IHS_QueueItemDestroyFunction *destroyFn;
    QueueNode *head;
    IHS_Mutex *mutex;
};

static IHS_QueueItem *ItemFromNode(QueueNode *node);

static QueueNode *NodeFromItem(IHS_QueueItem *item);

IHS_Queue *IHS_QueueCreate(size_t itemSize, IHS_QueueItemDestroyFunction *destroyFn) {
    IHS_Queue *queue = calloc(1, sizeof(IHS_Queue));
    queue->itemSize = itemSize;
    queue->destroyFn = destroyFn;
    queue->mutex = IHS_MutexCreate();
    return queue;
}

void IHS_QueueDestroy(IHS_Queue *queue) {
    IHS_MutexLock(queue->mutex);
    QueueNode *head = queue->head;
    QueueNode *tmp;
    while (head != NULL) {
        tmp = head;
        head = head->next;
        queue->destroyFn(ItemFromNode(tmp));
        free(tmp);
    }
    IHS_MutexUnlock(queue->mutex);

    IHS_MutexDestroy(queue->mutex);
    free(queue);
}

void IHS_QueueAppend(IHS_Queue *queue, IHS_QueueItem *item) {
    IHS_MutexLock(queue->mutex);
    QueueNode *node = NodeFromItem(item);
    assert(node->next == NULL);
    QueueNode *cur = queue->head;
    if (cur == NULL) {
        queue->head = node;
    } else {
        while (cur->next != NULL) {
            cur = cur->next;
        }
        cur->next = node;
    }
    IHS_MutexUnlock(queue->mutex);
}

IHS_QueueItem *IHS_QueuePoll(IHS_Queue *queue) {
    IHS_MutexLock(queue->mutex);
    QueueNode *head = queue->head;
    if (head == NULL) {
        IHS_MutexUnlock(queue->mutex);
        return NULL;
    }
    QueueNode *next = head->next;
    queue->head = next;
    IHS_MutexUnlock(queue->mutex);
    return ItemFromNode(head);
}

IHS_QueueItem *IHS_QueueItemObtain(IHS_Queue *queue) {
    QueueNode *item = calloc(1, sizeof(QueueNode) + queue->itemSize);
    return ItemFromNode(item);
}

void IHS_QueueItemFree(IHS_Queue *queue, IHS_QueueItem *item) {
    IHS_MutexLock(queue->mutex);
    queue->destroyFn(item);
    free(NodeFromItem(item));
    IHS_MutexUnlock(queue->mutex);
}

static IHS_QueueItem *ItemFromNode(QueueNode *node) {
    return (IHS_QueueItem *) ((void *) node + sizeof(QueueNode));
}

static QueueNode *NodeFromItem(IHS_QueueItem *item) {
    return (QueueNode *) ((void *) item - sizeof(QueueNode));
}
