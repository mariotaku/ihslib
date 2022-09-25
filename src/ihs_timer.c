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
#include <stdlib.h>
#include <time.h>

#include "ihs_timer.h"
#include "ihs_thread.h"

#define MAX_TIMERS_COUNT 16

struct IHS_Timers {
    IHS_Timer *timers[MAX_TIMERS_COUNT];
    IHS_Mutex *mutex;
};

struct IHS_Timer {
    IHS_Timers *timers;
    IHS_TimerRunFunction *run;
    IHS_TimerEndFunction *end;
    void *context;
    uint64_t nextExecution;
};

static bool TimerExecute(IHS_Timer *timer);

IHS_Timers *IHS_TimersCreate() {
    IHS_Timers *timers = calloc(1, sizeof(IHS_Timers));
    timers->mutex = IHS_MutexCreate();
    return timers;
}

void IHS_TimersDestroy(IHS_Timers *timers) {
    IHS_MutexLock(timers->mutex);
    for (int i = 0; i < MAX_TIMERS_COUNT; i++) {
        IHS_Timer *timer = timers->timers[i];
        if (!timer) continue;
        if (timer->end) {
            timer->end(timer->context);
        }
        free(timer);
        timers->timers[i] = NULL;
    }
    IHS_MutexUnlock(timers->mutex);
    IHS_MutexDestroy(timers->mutex);
    free(timers);
}

void IHS_TimersTick(IHS_Timers *timers) {
    IHS_MutexLock(timers->mutex);
    for (int i = 0; i < MAX_TIMERS_COUNT; i++) {
        IHS_Timer *timer = timers->timers[i];
        if (!timer) continue;
        if (!TimerExecute(timer)) {
            if (timer->end) {
                timer->end(timer->context);
            }
            free(timer);
            timers->timers[i] = NULL;
        }
    }
    IHS_MutexUnlock(timers->mutex);
}

IHS_Timer *IHS_TimerStart(IHS_Timers *timers, IHS_TimerRunFunction *run, IHS_TimerEndFunction *end,
                          uint64_t timeout, void *context) {
    IHS_MutexLock(timers->mutex);
    int availIndex = -1;
    for (int i = 0; i < MAX_TIMERS_COUNT; i++) {
        if (timers->timers[i]) continue;
        availIndex = i;
        break;
    }
    if (availIndex < 0) {
        IHS_MutexUnlock(timers->mutex);
        return NULL;
    }
    IHS_Timer *timer = calloc(1, sizeof(IHS_Timer));
    timer->timers = timers;
    timer->run = run;
    timer->end = end;
    timer->context = context;
    timer->nextExecution = IHS_TimerNow() + timeout;
    timers->timers[availIndex] = timer;
    IHS_MutexUnlock(timers->mutex);
    return timer;
}

void IHS_TimerStop(IHS_Timer *timer) {
    IHS_MutexLock(timer->timers->mutex);
    timer->nextExecution = 0;
    IHS_MutexUnlock(timer->timers->mutex);
}

void *IHS_TimerGetContext(IHS_Timer *timer) {
    return timer->context;
}

uint64_t IHS_TimerNow() {
    struct timespec tp;
    clock_gettime(CLOCK_MONOTONIC, &tp);
    return tp.tv_sec * 1000 + tp.tv_nsec / 1000000;
}

static bool TimerExecute(IHS_Timer *timer) {
    if (timer->nextExecution == 0) {
        return false;
    }
    if (timer->nextExecution > IHS_TimerNow()) {
        return true;
    }
    uint64_t timeout = timer->run(timer->context);
    if (timeout == 0) {
        return false;
    }
    timer->nextExecution = IHS_TimerNow() + timeout;
    return true;
}