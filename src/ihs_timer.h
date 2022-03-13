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

typedef struct IHS_Timers IHS_Timers;
typedef struct IHS_Timer IHS_Timer;

typedef uint64_t (IHS_TimerRunFunction)(void *context);

typedef void (IHS_TimerEndFunction)(void *context);

IHS_Timers *IHS_TimersCreate();

void IHS_TimersDestroy(IHS_Timers *timers);

void IHS_TimersTick(IHS_Timers *timers);

IHS_Timer *IHS_TimerStart(IHS_Timers *timers, IHS_TimerRunFunction *run, IHS_TimerEndFunction *end,
                          uint64_t timeout, void *context);

void IHS_TimerStop(IHS_Timer *timer);

void *IHS_TimerGetContext(IHS_Timer *timer);

uint64_t IHS_TimerNow();