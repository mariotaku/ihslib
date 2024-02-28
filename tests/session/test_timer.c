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
#include "ihs_timer.h"

typedef struct task_t {
    int timer;
    int id;
    int counter;
    int until;
} task_ctx_t;

static uint64_t task_run(int runCount, void *context);

static void task_end(void *context);

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

    sleep(1);
    IHS_TimerTaskStop(timer1_task1);
    assert(IHS_TimerTaskGetContext(timer1_task1) == &timer1_ctx1);
    sleep(1);

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