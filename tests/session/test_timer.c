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
#include <unistd.h>
#include <stdio.h>
#include <assert.h>
#include <stdatomic.h>
#include "ihs_timer.h"

typedef struct task_t {
    int timer;
    int id;
    // The counter is read from the main test thread and written from the timer
    // thread without any external lock; mark it atomic so TSan doesn't (correctly)
    // flag the access as a data race.
    atomic_int counter;
    int until;
} task_ctx_t;

static uint64_t task_run(int runCount, void *context);

static void task_end(void *context);

// For the re-entrancy regression test below.
typedef struct {
    IHS_Timer *timer;
    atomic_int reentrantCounter;
    atomic_int endFired;
} reentrant_ctx_t;

static uint64_t reentrant_inner_run(int runCount, void *context);
static void reentrant_end(void *context);

/**
 * Timers no longer have a thread of their own — whoever owns one drives it. This is a miniature of
 * what IHS_Base's worker does: sleep until the soonest deadline, then run whatever came due. It
 * exercises IHS_TimerNextDeadline as a side effect.
 */
static void pump(IHS_Timer **timers, size_t count, uint64_t durationMs) {
    uint64_t end = IHS_TimerNow() + durationMs;
    for (;;) {
        uint64_t now = IHS_TimerNow();
        if (now >= end) {
            break;
        }
        uint64_t wake = end;
        for (size_t i = 0; i < count; i++) {
            uint64_t deadline = IHS_TimerNextDeadline(timers[i]);
            if (deadline != 0 && deadline < wake) {
                wake = deadline;
            }
        }
        if (wake > now) {
            usleep((useconds_t) (wake - now) * 1000);
        }
        for (size_t i = 0; i < count; i++) {
            IHS_TimerRunPending(timers[i]);
        }
    }
}

int main(int argc, char *argv[]) {
    (void) argc;
    (void) argv;

    IHS_TimerInit();
    IHS_Timer *timer1 = IHS_TimerCreate();
    IHS_Timer *timer2 = IHS_TimerCreate();

    task_ctx_t timer1_ctx1 = {.timer = 1, .id = 2, .counter = 0};
    IHS_TimerTask *timer1_task1 = IHS_TimerTaskStart(timer1, task_run, task_end, 0, &timer1_ctx1);
    task_ctx_t timer1_ctx2 = {.timer = 1, .id = 1, .counter = 0, .until = 3};
    IHS_TimerTaskStart(timer1, task_run, NULL, 0, &timer1_ctx2);
    task_ctx_t timer2_ctx1 = {.timer = 2, .id = 1, .counter = 0, .until = 5};
    IHS_TimerTaskStart(timer2, task_run, NULL, 0, &timer2_ctx1);
    task_ctx_t timer2_ctx2 = {.timer = 2, .id = 2, .counter = 0, 7};
    IHS_TimerTaskStart(timer2, task_run, NULL, 0, &timer2_ctx2);

    IHS_Timer *timers[] = {timer1, timer2};
    pump(timers, 2, 1000);
    IHS_TimerTaskStop(timer1_task1);
    assert(IHS_TimerTaskGetContext(timer1_task1) == &timer1_ctx1);
    pump(timers, 2, 1000);

    // Regression: IHS_TimerTaskStopImmediate previously passed `timer` instead of `task`
    // to the queue predicate, so the task was never removed and kept firing.
    task_ctx_t stopImmCtx = {.timer = 99, .id = 99, .counter = 0, .until = 0};
    IHS_TimerTask *stopImmTask = IHS_TimerTaskStart(timer1, task_run, task_end, 100, &stopImmCtx);
    pump(timers, 2, 350);
    int beforeStop = stopImmCtx.counter;
    assert(beforeStop >= 2);
    IHS_TimerTaskStopImmediate(stopImmTask);
    pump(timers, 2, 350);
    assert(stopImmCtx.counter == beforeStop);

    // Contract guard: IHS_TimerTaskStopImmediate's end callback must be safe to call
    // back into the timer subsystem. Today the SDL mutex backend is recursive so even
    // a same-mutex re-entry would succeed, but the refactor (snapshot end+context
    // under the lock, invoke after unlock) protects against an AB-BA deadlock with
    // the timer worker thread (which acquires state.lock then timer->mutex). This
    // test exercises the re-entry path so any regression — including switching to a
    // non-recursive mutex backend — surfaces here.
    reentrant_ctx_t reentrantCtx = {.timer = timer1, .reentrantCounter = 0, .endFired = 0};
    IHS_TimerTask *outer = IHS_TimerTaskStart(timer1, task_run,
        reentrant_end, 1000000, &reentrantCtx);
    IHS_TimerTaskStopImmediate(outer);
    assert(reentrantCtx.endFired == 1);
    // Let the inner task (started from the end callback) fire at least once.
    pump(timers, 2, 150);
    assert(reentrantCtx.reentrantCounter >= 1);

    // Deadline reporting is what lets an owner block exactly as long as it should.
    IHS_Timer *timer3 = IHS_TimerCreate();
    assert(IHS_TimerNextDeadline(timer3) == 0 && "no tasks means wait indefinitely");
    task_ctx_t farCtx = {.timer = 3, .id = 1, .counter = 0, .until = 1};
    IHS_TimerTask *far = IHS_TimerTaskStart(timer3, task_run, NULL, 100000, &farCtx);
    uint64_t farDeadline = IHS_TimerNextDeadline(timer3);
    assert(farDeadline > IHS_TimerNow());
    task_ctx_t nearCtx = {.timer = 3, .id = 2, .counter = 0, .until = 1};
    IHS_TimerTaskStart(timer3, task_run, NULL, 50, &nearCtx);
    uint64_t nearDeadline = IHS_TimerNextDeadline(timer3);
    assert(nearDeadline < farDeadline && "the soonest task sets the deadline");
    // A task asked to stop is reaped, not waited for.
    IHS_TimerTaskStop(far);
    assert(IHS_TimerNextDeadline(timer3) == nearDeadline);
    IHS_Timer *justTimer3[] = {timer3};
    pump(justTimer3, 1, 200);
    assert(nearCtx.counter == 1);
    assert(IHS_TimerNextDeadline(timer3) == 0 && "everything finished, so wait indefinitely again");
    IHS_TimerDestroy(timer3);

    IHS_TimerDestroy(timer1);
    IHS_TimerDestroy(timer2);
    IHS_TimerQuit();

    assert(timer1_ctx1.counter < 20);
    assert(timer1_ctx2.counter == 3);
    assert(timer2_ctx1.counter == 5);
    assert(timer2_ctx2.counter == 7);
    return 0;
}

static uint64_t task_run(int runCount, void *context) {
    task_ctx_t *task = context;
    assert(runCount == task->counter);
    task->counter++;
    printf("Timer #%d task %d counter %d\n", task->timer, task->id, task->counter);
    if (task->until > 0 && task->counter >= task->until) {
        return 0;
    }
    return 100;
}

static void task_end(void *context) {
    (void) context;
    printf("Timer has ended\n");
}

static uint64_t reentrant_inner_run(int runCount, void *context) {
    (void) runCount;
    reentrant_ctx_t *ctx = context;
    ctx->reentrantCounter++;
    if (ctx->reentrantCounter >= 3) return 0;
    return 50;
}

static void reentrant_end(void *context) {
    reentrant_ctx_t *ctx = context;
    ctx->endFired++;
    // Re-enter the timer subsystem from inside the end callback. Pre-fix this
    // would deadlock on timer->mutex; post-fix it succeeds because StopImmediate
    // released the lock before invoking us.
    IHS_TimerTaskStart(ctx->timer, reentrant_inner_run, NULL, 50, ctx);
}