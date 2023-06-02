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

#include "ihs_arraylist.h"

#include <stdlib.h>
#include <memory.h>
#include <assert.h>

void IHS_ArrayListInit(IHS_ArrayList *list, size_t itemSize) {
    assert(itemSize >= 1);
    memset(list, 0, sizeof(IHS_ArrayList));
    list->itemSize = itemSize;
    list->size = 0;
    list->capacity = 16;
    list->data = calloc(list->capacity, list->itemSize);
}

void IHS_ArrayListDeinit(IHS_ArrayList *list) {
    if (list->data != NULL) {
        free(list->data);
    }
    memset(list, 0, sizeof(IHS_ArrayList));
}

void *IHS_ArrayListAppend(IHS_ArrayList *list, const void *itemPtr) {
    if (list->size + 1 >= list->capacity) {
        list->capacity = list->capacity * 2;
        void *allocated = realloc(list->data, list->capacity * list->itemSize);
        assert(allocated != NULL);
        list->data = allocated;
    }
    void *elementPtr = IHS_ArrayListGet(list, list->size);
    if (itemPtr != NULL) {
        memcpy(elementPtr, itemPtr, list->itemSize);
    } else {
        memset(elementPtr, 0, list->itemSize);
    }
    list->size += 1;
    return elementPtr;
}

bool IHS_ArrayListRemoveFirst(IHS_ArrayList *list, const void *itemPtr) {
    for (int i = 0; i < list->size; ++i) {
        if (memcmp(IHS_ArrayListGet(list, i), itemPtr, list->itemSize) == 0) {
            int toMove = (int) ((list->size - 1 - i) * list->itemSize);
            if (toMove > 0) {
                memmove(IHS_ArrayListGet(list, i), IHS_ArrayListGet(list, i + 1), toMove);
            }
            list->size -= 1;
            return true;
        }
    }
    return false;
}

bool IHS_ArrayListRemove(IHS_ArrayList *list, size_t index) {
    if (index >= list->size) {
        return false;
    }
    int toMove = (int) ((list->size - 1 - index) * list->itemSize);
    memmove(IHS_ArrayListGet(list, index), IHS_ArrayListGet(list, index + 1), toMove);
    list->size -= 1;
    return true;
}

void *IHS_ArrayListGet(IHS_ArrayList *list, size_t index) {
    assert(list->data != NULL);
    return list->data + index * list->itemSize;
}

int IHS_ArrayListBinarySearch(const IHS_ArrayList *list, const void *value, IHS_ArrayListSearchFn searchFn) {
    void *itemPtr = bsearch(value, list->data, list->size, list->itemSize, searchFn);
    if (itemPtr == NULL) {
        return -1;
    }
    return (int) ((itemPtr - list->data) / list->itemSize);
}

int IHS_ArrayListLinearSearch(const IHS_ArrayList *list, const void *value, IHS_ArrayListSearchFn searchFn) {
    for (size_t i = 0, j = list->size; i < j; ++i) {
        if (searchFn(value, list->data + i * list->itemSize) == 0) {
            return (int) i;
        }
    }
    return -1;
}

void IHS_ArrayListClear(IHS_ArrayList *list) {
    list->size = 0;
}