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

#include <stdint.h>

typedef struct IHS_Timer IHS_Timer;
typedef struct IHS_TimerTask IHS_TimerTask;

typedef uint64_t (IHS_TimerRunFunction)(int runCount, void *context);

typedef void (IHS_TimerEndFunction)(void *context);

void IHS_TimerInit();

void IHS_TimerQuit();

/**
 * Create tasks instance. Start timer thread if not started
 * @return Timers instance
 */
IHS_Timer *IHS_TimerCreate();

/**
 * Destroy tasks instance. If all references are removed, destroy the timer
 * @param timer
 */
void IHS_TimerDestroy(IHS_Timer *timer);

IHS_TimerTask *IHS_TimerTaskStart(IHS_Timer *timer, IHS_TimerRunFunction *run, IHS_TimerEndFunction *end,
                                  uint64_t timeout, void *context);

/**
 * Ask a timer task to stop. It will be ended and freed in next timer loop.
 * @param task Timer task to stop
 */
void IHS_TimerTaskStop(IHS_TimerTask *task);

/**
 * Stop and free the timer task immediately.
 * @param task Timer task to stop
 */
void IHS_TimerTaskStopImmediate(IHS_TimerTask *task);

void *IHS_TimerTaskGetContext(IHS_TimerTask *task);

int IHS_TimerTaskGetRunCount(const IHS_TimerTask *task);

uint64_t IHS_TimerNow();