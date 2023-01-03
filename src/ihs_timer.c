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

#include <stdbool.h>
#include <time.h>
#include <assert.h>
#include <unistd.h>

#include "ihs_timer.h"
#include "ihs_thread.h"
#include "ihs_queue.h"

struct IHS_Timer {
    IHS_Queue *tasks;
    IHS_Mutex *mutex;
};

struct IHS_TimerTask {
    IHS_Timer *timer;
    IHS_TimerRunFunction *run;
    IHS_TimerEndFunction *end;
    void *context;
    uint64_t nextExecution;
    int runCount;
};

static struct {
    IHS_Queue *timers;
    IHS_Mutex *lock;
    IHS_Thread *thread;
} state = {NULL, NULL, NULL};

static void TimerThreadWorker();

/*
 * Timer functions
 */


static bool TimerExecute(IHS_Timer *timer, void *context);

static bool TimerEquals(IHS_Timer *timer, void *context);

static void TimerDestroy(IHS_Timer *timer, void *context);

/*
 * Task functions
 */

static bool TaskExecute(IHS_TimerTask *task, IHS_Timer *timer);

static void TaskDestroy(IHS_TimerTask *task, IHS_Timer *timer);

void IHS_TimerInit() {
    state.lock = IHS_MutexCreate();
    state.timers = IHS_QueueCreate(sizeof(IHS_Timer));
}

void IHS_TimerQuit() {
    IHS_MutexLock(state.lock);
    IHS_QueueDestroy(state.timers, (IHS_QueueConsumerFunction *) TimerDestroy, NULL);
    IHS_MutexUnlock(state.lock);
    IHS_MutexDestroy(state.lock);
}

IHS_Timer *IHS_TimerCreate() {
    IHS_MutexLock(state.lock);
    IHS_Timer *timer = (IHS_Timer *) IHS_QueueItemObtain(state.timers);
    timer->tasks = IHS_QueueCreate(sizeof(IHS_TimerTask));
    timer->mutex = IHS_MutexCreate();
    IHS_QueueAppend(state.timers, (IHS_QueueItem *) timer);
    if (state.thread == NULL) {
        state.thread = IHS_ThreadCreate(TimerThreadWorker, "IHS.Timer", NULL);
    }
    IHS_MutexUnlock(state.lock);
    return (IHS_Timer *) timer;
}

void IHS_TimerDestroy(IHS_Timer *timer) {
    IHS_MutexLock(state.lock);

    IHS_Timer *matched = (IHS_Timer *) IHS_QueuePollBy(state.timers, (IHS_QueuePredicateFunction *) TimerEquals,
                                                       timer);
    assert(matched == timer);
    TimerDestroy(matched, NULL);
    IHS_QueueItemFree((IHS_QueueItem *) matched);
    // All timer are removed
    if (IHS_QueueIsEmpty(state.timers)) {
        IHS_MutexUnlock(state.lock);
        IHS_ThreadJoin(state.thread);

        IHS_MutexLock(state.lock);
        state.thread = NULL;
    }
    IHS_MutexUnlock(state.lock);
}

IHS_TimerTask *IHS_TimerTaskStart(IHS_Timer *timer, IHS_TimerRunFunction *run, IHS_TimerEndFunction *end,
                                  uint64_t timeout, void *context) {
    IHS_MutexLock(timer->mutex);
    if (timer->tasks == NULL) {
        IHS_MutexUnlock(timer->mutex);
        return NULL;
    }
    IHS_TimerTask *task = (IHS_TimerTask *) IHS_QueueItemObtain(timer->tasks);
    task->timer = timer;
    task->run = run;
    task->end = end;
    task->context = context;
    task->nextExecution = IHS_TimerNow() + timeout;
    IHS_QueueAppend(timer->tasks, (IHS_QueueItem *) task);
    IHS_MutexUnlock(timer->mutex);
    return task;
}

void IHS_TimerTaskStop(IHS_TimerTask *task) {
    IHS_MutexLock(task->timer->mutex);
    task->nextExecution = 0;
    IHS_MutexUnlock(task->timer->mutex);
}

void *IHS_TimerTaskGetContext(IHS_TimerTask *task) {
    return task->context;
}

int IHS_TimerTaskGetRunCount(const IHS_TimerTask *task) {
    return task->runCount;
}

uint64_t IHS_TimerNow() {
    struct timespec tp;
    clock_gettime(CLOCK_MONOTONIC, &tp);
    return tp.tv_sec * 1000 + tp.tv_nsec / 1000000;
}

static void TimerThreadWorker() {
    size_t iterated;
    do {
        IHS_MutexLock(state.lock);
        iterated = IHS_QueuePollEach(state.timers, (IHS_QueuePredicateFunction *) TimerExecute, NULL,
                                     (IHS_QueueConsumerFunction *) TimerDestroy, NULL);
        IHS_MutexUnlock(state.lock);
        usleep(1000);
    } while (iterated > 0);
}

/*
 * Timer functions
 */

static bool TimerExecute(IHS_Timer *timer, void *context) {
    (void) context;
    if (timer->tasks == NULL) {
        return true;
    }
    IHS_MutexLock(timer->mutex);
    IHS_QueuePollEach(timer->tasks, (IHS_QueuePredicateFunction *) TaskExecute, timer,
                      (IHS_QueueConsumerFunction *) TaskDestroy, timer);
    IHS_MutexUnlock(timer->mutex);
    return false;
}

static bool TimerEquals(IHS_Timer *timer, void *context) {
    return (void *) timer == context;
}

static void TimerDestroy(IHS_Timer *timer, void *context) {
    (void) context;
    IHS_MutexLock(timer->mutex);
    IHS_QueueDestroy(timer->tasks, (IHS_QueueConsumerFunction *) TaskDestroy, timer);
    timer->tasks = NULL;
    IHS_MutexUnlock(timer->mutex);
    IHS_MutexDestroy(timer->mutex);
}

/*
 * Task functions
 */

static bool TaskExecute(IHS_TimerTask *task, IHS_Timer *timer) {
    (void) timer;
    if (task->nextExecution == 0) {
        return true;
    }
    if (task->nextExecution > IHS_TimerNow()) {
        return false;
    }
    uint64_t timeout = task->run(task->runCount, task->context);
    task->runCount += 1;
    if (timeout == 0) {
        return true;
    }
    task->nextExecution = IHS_TimerNow() + timeout;
    return false;
}

static void TaskDestroy(IHS_TimerTask *task, IHS_Timer *timer) {
    (void) timer;
    if (task->end) {
        task->end(task->context);
    }
}