/*
 *  _____  _   _  _____  _  _  _     
 * |_   _|| | | |/  ___|| |(_)| |     Steam    
 *   | |  | |_| |\ `--. | | _ | |__     In-Home
 *   | |  |  _  | `--. \| || || '_ \      Streaming
 *  _| |_ | | | |/\__/ /| || || |_) |       Library
 *  \___/ \_| |_/\____/ |_||_||_.__/
 *
 * Copyright (c) 2022 Mariotaku <https://github.com/mariotaku>.
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

#include <stdio.h>
#include <assert.h>
#include "ihs_enumeration.h"

typedef struct LinkedListNode {
    int value;
    struct LinkedListNode *next;
} LinkedListNode;

static void *LinkedListNext(void *cur);

static void TestArray();

static void TestLinkedList();

static void NoOp();

int main(int argc, char *argv[]) {
    TestArray();
    TestLinkedList();
    return 0;
}

static void TestArray() {
    printf("%s:\n", __FUNCTION__);

    int array[] = {1, 2, 3, 4, 5, 6, 7, 8};
    IHS_Enumeration *e = IHS_EnumerationArrayCreate(array, sizeof(int), 8, NoOp);
    assert(IHS_EnumerationCount(e) == 8);
    int iterations = 0;
    for (IHS_EnumerationReset(e); !IHS_EnumerationEnded(e); IHS_EnumerationNext(e)) {
        int *v = IHS_EnumerationGet(e);
        printf("%d\n", *v);
        iterations++;
    }
    assert(iterations == 8);
    assert(IHS_EnumerationGet(e) == NULL);
    IHS_EnumerationFree(e);
}

static void TestLinkedList() {
    printf("%s:\n", __FUNCTION__);

    LinkedListNode node7 = {.value = 8};
    LinkedListNode node6 = {.value = 7, .next = &node7};
    LinkedListNode node5 = {.value = 6, .next = &node6};
    LinkedListNode node4 = {.value = 5, .next = &node5};
    LinkedListNode node3 = {.value = 4, .next = &node4};
    LinkedListNode node2 = {.value = 3, .next = &node3};
    LinkedListNode node1 = {.value = 2, .next = &node2};
    LinkedListNode node0 = {.value = 1, .next = &node1};

    IHS_Enumeration *e = IHS_EnumerationLinkedListCreate(&node0, LinkedListNext, NoOp);
    assert(IHS_EnumerationCount(e) == 8);
    int iterations = 0;
    for (IHS_EnumerationReset(e); !IHS_EnumerationEnded(e); IHS_EnumerationNext(e)) {
        LinkedListNode *node = IHS_EnumerationGet(e);
        printf("%d\n", node->value);
        iterations++;
    }
    assert(iterations == 8);
    assert(IHS_EnumerationGet(e) == NULL);
    IHS_EnumerationFree(e);
}

static void *LinkedListNext(void *cur) {
    return ((LinkedListNode *) cur)->next;
}

static void NoOp() {

}