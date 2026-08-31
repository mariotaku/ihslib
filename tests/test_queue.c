/*
 *  _____  _   _  _____  _  _  _
 * |_   _|| | | |/  ___|| |(_)| |     Steam
 *   | |  | |_| |\ `--. | | _ | |__     In-Home
 *   | |  |  _  | `--. \| || || '_ \      Streaming
 *  _| |_ | | | |/\__/ /| || || |_) |       Library
 *  \___/ \_| |_/\____/ |_||_||_.__/
 *
 * Copyright (c) 2026 Mariotaku <https://github.com/mariotaku>.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 */

#include <assert.h>
#include <stdio.h>

#include "ihs_queue.h"

typedef struct Item {
    int value;
} Item;

static IHS_QueueItem *Push(IHS_Queue *queue, int value) {
    IHS_QueueItem *item = IHS_QueueItemObtain(queue);
    ((Item *) item)->value = value;
    IHS_QueueAppend(queue, item);
    return item;
}

static int PollValue(IHS_Queue *queue) {
    IHS_QueueItem *item = IHS_QueuePoll(queue);
    assert(item != NULL);
    int value = ((Item *) item)->value;
    IHS_QueueItemFree(item);
    return value;
}

static bool MatchValue(IHS_QueueItem *item, void *context) {
    return ((Item *) item)->value == *((int *) context);
}

static void DestroyItem(IHS_QueueItem *item, void *context) {
    (void) item;
    (*(int *) context)++;
}

static void test_fifo_order(void) {
    IHS_Queue *queue = IHS_QueueCreate(sizeof(Item));
    assert(IHS_QueueIsEmpty(queue));
    for (int i = 0; i < 8; i++) {
        Push(queue, i);
    }
    assert(!IHS_QueueIsEmpty(queue));
    for (int i = 0; i < 8; i++) {
        assert(PollValue(queue) == i);
    }
    assert(IHS_QueueIsEmpty(queue));
    assert(IHS_QueuePoll(queue) == NULL);
    IHS_QueueDestroy(queue, NULL, NULL);
}

static void test_append_after_draining(void) {
    // The tail pointer must be cleared when the last item is polled, otherwise the next
    // append would link onto a freed node.
    IHS_Queue *queue = IHS_QueueCreate(sizeof(Item));
    Push(queue, 1);
    assert(PollValue(queue) == 1);
    assert(IHS_QueueIsEmpty(queue));
    Push(queue, 2);
    Push(queue, 3);
    assert(PollValue(queue) == 2);
    assert(PollValue(queue) == 3);
    assert(IHS_QueueIsEmpty(queue));
    IHS_QueueDestroy(queue, NULL, NULL);
}

static void test_poll_by_tail_then_append(void) {
    // PollBy removing the tail must move the tail back to its predecessor.
    IHS_Queue *queue = IHS_QueueCreate(sizeof(Item));
    Push(queue, 10);
    Push(queue, 20);
    Push(queue, 30);
    int wanted = 30;
    IHS_QueueItem *polled = IHS_QueuePollBy(queue, MatchValue, &wanted);
    assert(polled != NULL);
    assert(((Item *) polled)->value == 30);
    IHS_QueueItemFree(polled);

    Push(queue, 40);
    assert(PollValue(queue) == 10);
    assert(PollValue(queue) == 20);
    assert(PollValue(queue) == 40);
    assert(IHS_QueueIsEmpty(queue));

    wanted = 99;
    assert(IHS_QueuePollBy(queue, MatchValue, &wanted) == NULL);
    IHS_QueueDestroy(queue, NULL, NULL);
}

static void test_poll_each_removing_tail(void) {
    IHS_Queue *queue = IHS_QueueCreate(sizeof(Item));
    Push(queue, 1);
    Push(queue, 2);
    Push(queue, 3);
    int wanted = 3, destroyed = 0;
    assert(IHS_QueuePollEach(queue, MatchValue, &wanted, DestroyItem, &destroyed) == 3);
    assert(destroyed == 1);

    Push(queue, 4);
    assert(PollValue(queue) == 1);
    assert(PollValue(queue) == 2);
    assert(PollValue(queue) == 4);
    assert(IHS_QueueIsEmpty(queue));
    IHS_QueueDestroy(queue, NULL, NULL);
}

static void test_poll_each_removing_all(void) {
    IHS_Queue *queue = IHS_QueueCreate(sizeof(Item));
    Push(queue, 1);
    Push(queue, 2);
    int destroyed = 0;
    int wanted = 1;
    assert(IHS_QueuePollEach(queue, MatchValue, &wanted, DestroyItem, &destroyed) == 2);
    wanted = 2;
    assert(IHS_QueuePollEach(queue, MatchValue, &wanted, DestroyItem, &destroyed) == 1);
    assert(destroyed == 2);
    assert(IHS_QueueIsEmpty(queue));

    Push(queue, 5);
    assert(PollValue(queue) == 5);
    IHS_QueueDestroy(queue, NULL, NULL);
}

int main(void) {
    test_fifo_order();
    test_append_after_draining();
    test_poll_by_tail_then_append();
    test_poll_each_removing_tail();
    test_poll_each_removing_all();
    printf("queue tests OK\n");
    return 0;
}
