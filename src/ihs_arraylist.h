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

#include <stdbool.h>
#include <stddef.h>

typedef struct IHS_ArrayList {
    void *data;
    size_t itemSize;
    size_t size;
    size_t capacity;
} IHS_ArrayList;

typedef int (*IHS_ArrayListSearchFn)(const void *searchValue, const void *item);

IHS_ArrayList *IHS_ArrayListCreate(size_t itemSize);

void IHS_ArrayListInit(IHS_ArrayList *list, size_t itemSize);

void IHS_ArrayListDeinit(IHS_ArrayList *list);

/**
 *
 * @param list
 * @param itemPtr If not null, memory of this pointer will be copied
 * @return Pointer of the element in the array
 */
void *IHS_ArrayListAppend(IHS_ArrayList *list, const void *itemPtr);

bool IHS_ArrayListRemoveFirst(IHS_ArrayList *list, const void *itemPtr);

bool IHS_ArrayListRemove(IHS_ArrayList *list, size_t index);

void *IHS_ArrayListGet(IHS_ArrayList *list, size_t index);

/**
 * Binary search
 *
 * @param list
 * @param value
 * @param searchFn
 * @return
 */
int IHS_ArrayListBinarySearch(const IHS_ArrayList *list, const void *value, IHS_ArrayListSearchFn searchFn);

/**
 * Linear search
 *
 * @param list
 * @param value
 * @param searchFn
 * @return
 */
int IHS_ArrayListLinearSearch(const IHS_ArrayList *list, const void *value, IHS_ArrayListSearchFn searchFn);

void IHS_ArrayListClear(IHS_ArrayList *list);