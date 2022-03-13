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

/**
 * @file ihs_queue.h
 * @brief Thread safe event queue
 */

typedef struct IHS_Queue IHS_Queue;
typedef struct IHS_QueueItem IHS_QueueItem;

typedef void (IHS_QueueItemDestroyFunction)(IHS_QueueItem *item);

IHS_Queue *IHS_QueueCreate(size_t itemSize, IHS_QueueItemDestroyFunction *destroyFn);

void IHS_QueueDestroy(IHS_Queue *queue);

void IHS_QueueAppend(IHS_Queue *queue, IHS_QueueItem *item);

IHS_QueueItem *IHS_QueuePoll(IHS_Queue *queue);

IHS_QueueItem *IHS_QueueItemObtain(IHS_Queue *queue);

void IHS_QueueItemFree(IHS_Queue *queue, IHS_QueueItem *item);