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

#include "ihs_queue.h"

#include <stdlib.h>
#include <assert.h>

typedef struct QueueNode {
    struct QueueNode *next;
} QueueNode;

struct IHS_Queue {
    size_t itemSize;
    QueueNode *head;
};

static IHS_QueueItem *ItemFromNode(QueueNode *node);

static QueueNode *NodeFromItem(IHS_QueueItem *item);

IHS_Queue *IHS_QueueCreate(size_t itemSize) {
    IHS_Queue *queue = calloc(1, sizeof(IHS_Queue));
    queue->itemSize = itemSize;
    return queue;
}

void IHS_QueueDestroy(IHS_Queue *queue, IHS_QueueConsumerFunction *destroy, void *destroyContext) {
    assert(queue != NULL);
    QueueNode *head = queue->head;
    QueueNode *tmp;
    while (head != NULL) {
        tmp = head;
        head = head->next;
        if (destroy != NULL) {
            destroy(ItemFromNode(tmp), destroyContext);
        }
        free(tmp);
    }
    free(queue);
}

void IHS_QueueAppend(IHS_Queue *queue, IHS_QueueItem *item) {
    assert(queue != NULL);
    assert(item != NULL);
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
}

IHS_QueueItem *IHS_QueuePoll(IHS_Queue *queue) {
    assert(queue != NULL);
    QueueNode *head = queue->head;
    if (head == NULL) {
        return NULL;
    }
    QueueNode *next = head->next;
    queue->head = next;

    head->next = NULL;
    return ItemFromNode(head);
}

IHS_QueueItem *IHS_QueuePollBy(IHS_Queue *queue, IHS_QueuePredicateFunction *predicate, void *predicateContext) {
    assert(queue != NULL);
    assert(predicate != NULL);
    QueueNode *cur = queue->head, *prev = NULL;
    while (cur != NULL) {
        IHS_QueueItem *item = ItemFromNode(cur);
        QueueNode *next = cur->next;
        if (predicate(item, predicateContext)) {
            if (prev != NULL) {
                prev->next = next;
            } else {
                queue->head = next;
            }
            return item;
        }
        prev = cur;
        cur = next;
    }
    return NULL;
}

size_t IHS_QueuePollEach(IHS_Queue *queue, IHS_QueuePredicateFunction *predicate, void *predicateContext,
                         IHS_QueueConsumerFunction *destroy, void *destroyContext) {
    assert(queue != NULL);
    assert(predicate != NULL);
    assert(destroy != NULL);
    size_t iterated = 0;
    QueueNode *cur = queue->head, *prev = NULL;
    while (cur != NULL) {
        IHS_QueueItem *item = ItemFromNode(cur);
        QueueNode *next = cur->next;
        if (predicate(item, predicateContext)) {
            if (prev != NULL) {
                prev->next = next;
            } else {
                queue->head = next;
            }
            destroy(item, destroyContext);
            free(cur);
        } else {
            prev = cur;
        }
        cur = next;
        iterated++;
    }
    return iterated;
}


bool IHS_QueueIsEmpty(const IHS_Queue *queue) {
    assert(queue != NULL);
    return queue->head == NULL;
}

IHS_QueueItem *IHS_QueueItemObtain(IHS_Queue *queue) {
    assert(queue != NULL);
    QueueNode *item = calloc(1, sizeof(QueueNode) + queue->itemSize);
    return ItemFromNode(item);
}

void IHS_QueueItemFree(IHS_QueueItem *item) {
    assert(item != NULL);
    free(NodeFromItem(item));
}

static IHS_QueueItem *ItemFromNode(QueueNode *node) {
    assert(node != NULL);
    return (IHS_QueueItem *) ((void *) node + sizeof(QueueNode));
}

static QueueNode *NodeFromItem(IHS_QueueItem *item) {
    assert(item != NULL);
    return (QueueNode *) ((void *) item - sizeof(QueueNode));
}

