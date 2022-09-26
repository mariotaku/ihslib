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

#include <stddef.h>
#include <stdbool.h>

/**
 * @file ihs_queue.h
 * @brief Linked-list based queue
 * @attention This queue is not thread safe!
 */

typedef struct IHS_Queue IHS_Queue;
typedef struct IHS_QueueItem IHS_QueueItem;

typedef void (IHS_QueueConsumerFunction)(IHS_QueueItem *item, void *context);

typedef bool (IHS_QueuePredicateFunction)(IHS_QueueItem *item, void *context);

IHS_Queue *IHS_QueueCreate(size_t itemSize);

/**
 * Remove all items in the queue and free it. All allocated items will be freed too.
 * @param queue Queue instance
 * @param destroy
 * @param destroyContext
 */
void IHS_QueueDestroy(IHS_Queue *queue, IHS_QueueConsumerFunction *destroy, void *destroyContext);

/**
 * Append an item allocated to the end of the queue.
 * @param queue Queue instance
 * @param item
 */
void IHS_QueueAppend(IHS_Queue *queue, IHS_QueueItem *item);

/**
 * Remove the item at the beginning of the queue.
 * @param queue Queue instance
 * @return
 */
IHS_QueueItem *IHS_QueuePoll(IHS_Queue *queue);

/**
 * Find queue item by matching predicate, and remove it from the queue.
 * @param queue Queue instance
 * @param predicate
 * @param predicateContext
 * @return
 */
IHS_QueueItem *IHS_QueuePollBy(IHS_Queue *queue, IHS_QueuePredicateFunction *predicate, void *predicateContext);

/**
 * Iterate through all items by calling \p predicate. If it returns true, remove the item from the queue after calling
 * \p destroy. The memory allocated for item will be freed.
 * @param queue
 * @param predicate
 * @param predicateContext
 * @param destroy
 * @param destroyContext
 * @return
 */
size_t IHS_QueuePollEach(IHS_Queue *queue, IHS_QueuePredicateFunction *predicate, void *predicateContext,
                         IHS_QueueConsumerFunction *destroy, void *destroyContext);

bool IHS_QueueIsEmpty(const IHS_Queue *queue);

IHS_QueueItem *IHS_QueueItemObtain(IHS_Queue *queue);

void IHS_QueueItemFree(IHS_QueueItem *item);