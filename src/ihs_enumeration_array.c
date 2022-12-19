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
#include "ihs_enumeration.h"

typedef struct IHS_EnumerationArray {
    IHS_Enumeration base;
    void *array;
    size_t itemSize, count, index;
    IHS_EnumerationFreeUnderlying arrFree;
} IHS_EnumerationArray;

static IHS_Enumeration *EnumerationArrayAlloc(const IHS_EnumerationClass *cls, void *arg);

static void EnumerationArrayFree(IHS_Enumeration *enumeration);

static size_t EnumerationArrayCount(const IHS_Enumeration *enumeration);

static void EnumerationArrayReset(IHS_Enumeration *enumeration);

static bool EnumerationArrayEnded(const IHS_Enumeration *enumeration);

static void *EnumerationArrayGet(const IHS_Enumeration *enumeration);

static void *EnumerationArrayNext(IHS_Enumeration *enumeration);

const static IHS_EnumerationClass ArrayClass = {
        .alloc = EnumerationArrayAlloc,
        .free = EnumerationArrayFree,
        .count = EnumerationArrayCount,
        .reset = EnumerationArrayReset,
        .ended = EnumerationArrayEnded,
        .get = EnumerationArrayGet,
        .next = EnumerationArrayNext,
};

IHS_Enumeration *IHS_EnumerationArrayCreate(void *array, size_t itemSize, size_t count,
                                            IHS_EnumerationFreeUnderlying free) {
    IHS_EnumerationArray *enumeration = (IHS_EnumerationArray *) IHS_EnumerationCreate(&ArrayClass, NULL);
    enumeration->array = array;
    enumeration->itemSize = itemSize;
    enumeration->count = count;
    enumeration->arrFree = free;
    return (IHS_Enumeration *) enumeration;
}

static IHS_Enumeration *EnumerationArrayAlloc(const IHS_EnumerationClass *cls, void *arg) {
    (void) arg;
    IHS_Enumeration *enumeration = calloc(1, sizeof(IHS_EnumerationArray));
    enumeration->cls = cls;
    return enumeration;
}

static void EnumerationArrayFree(IHS_Enumeration *enumeration) {
    IHS_EnumerationArray *ll = (IHS_EnumerationArray *) enumeration;
    if (ll->arrFree != NULL) {
        ll->arrFree(ll->array);
    }
    free(enumeration);
}

static size_t EnumerationArrayCount(const IHS_Enumeration *enumeration) {
    const IHS_EnumerationArray *ll = (const IHS_EnumerationArray *) enumeration;
    return ll->count;
}

static void EnumerationArrayReset(IHS_Enumeration *enumeration) {
    IHS_EnumerationArray *ll = (IHS_EnumerationArray *) enumeration;
    ll->index = 0;
}

static bool EnumerationArrayEnded(const IHS_Enumeration *enumeration) {
    IHS_EnumerationArray *ll = (IHS_EnumerationArray *) enumeration;
    return ll->index >= ll->count;
}

static void *EnumerationArrayGet(const IHS_Enumeration *enumeration) {
    IHS_EnumerationArray *ll = (IHS_EnumerationArray *) enumeration;
    if (ll->index >= ll->count) {
        return NULL;
    }
    return ll->array + ll->index * ll->itemSize;
}

static void *EnumerationArrayNext(IHS_Enumeration *enumeration) {
    IHS_EnumerationArray *ll = (IHS_EnumerationArray *) enumeration;
    ll->index++;
    return EnumerationArrayGet(enumeration);
}