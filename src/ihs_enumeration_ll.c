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

typedef struct IHS_EnumerationLinkedList {
    IHS_Enumeration base;
    void *head;
    void *cur;
    IHS_EnumerationLinkedListNext llNext;
    IHS_EnumerationFreeUnderlying llFree;
} IHS_EnumerationLinkedList;

static IHS_Enumeration *EnumerationLLAlloc(const IHS_EnumerationClass *cls);

static void EnumerationLLFree(IHS_Enumeration *enumeration);

static size_t EnumerationLLCount(const IHS_Enumeration *enumeration);

static void EnumerationLLReset(IHS_Enumeration *enumeration);

static bool EnumerationLLEnded(const IHS_Enumeration *enumeration);

static void *EnumerationLLGet(const IHS_Enumeration *enumeration);

static void *EnumerationLLNext(IHS_Enumeration *enumeration);

const static IHS_EnumerationClass LinkedListClass = {
        .alloc = EnumerationLLAlloc,
        .free = EnumerationLLFree,
        .count = EnumerationLLCount,
        .reset = EnumerationLLReset,
        .ended = EnumerationLLEnded,
        .get = EnumerationLLGet,
        .next = EnumerationLLNext,
};

IHS_Enumeration *IHS_EnumerationLinkedListCreate(void *ll, IHS_EnumerationLinkedListNext next,
                                                 IHS_EnumerationFreeUnderlying free) {
    assert(next != NULL);
    IHS_EnumerationLinkedList *enumeration = (IHS_EnumerationLinkedList *) IHS_EnumerationCreate(&LinkedListClass);
    enumeration->head = ll;
    enumeration->llNext = next;
    enumeration->llFree = free;
    return (IHS_Enumeration *) enumeration;
}

static IHS_Enumeration *EnumerationLLAlloc(const IHS_EnumerationClass *cls) {
    IHS_Enumeration *enumeration = calloc(1, sizeof(IHS_EnumerationLinkedList));
    enumeration->cls = cls;
    return enumeration;
}

static void EnumerationLLFree(IHS_Enumeration *enumeration) {
    IHS_EnumerationLinkedList *ll = (IHS_EnumerationLinkedList *) enumeration;
    if (ll->llFree != NULL) {
        ll->llFree(ll->head);
    }
    free(enumeration);
}

static size_t EnumerationLLCount(const IHS_Enumeration *enumeration) {
    const IHS_EnumerationLinkedList *ll = (const IHS_EnumerationLinkedList *) enumeration;
    size_t count = 0;
    for (void *cur = ll->head; cur != NULL; cur = ll->llNext(cur)) {
        count++;
    }
    return count;
}

static void EnumerationLLReset(IHS_Enumeration *enumeration) {
    IHS_EnumerationLinkedList *ll = (IHS_EnumerationLinkedList *) enumeration;
    ll->cur = ll->head;
}

static bool EnumerationLLEnded(const IHS_Enumeration *enumeration) {
    IHS_EnumerationLinkedList *ll = (IHS_EnumerationLinkedList *) enumeration;
    return ll->cur == NULL;
}

static void *EnumerationLLGet(const IHS_Enumeration *enumeration) {
    IHS_EnumerationLinkedList *ll = (IHS_EnumerationLinkedList *) enumeration;
    return ll->cur;
}

static void *EnumerationLLNext(IHS_Enumeration *enumeration) {
    IHS_EnumerationLinkedList *ll = (IHS_EnumerationLinkedList *) enumeration;
    ll->cur = ll->llNext(ll->cur);
    return ll->cur;
}