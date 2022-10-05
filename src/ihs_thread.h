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
#include <stdbool.h>

typedef struct IHS_Thread IHS_Thread;
typedef struct IHS_Mutex IHS_Mutex;
typedef struct IHS_Cond IHS_Cond;

typedef void (IHS_ThreadFunction)(void *context);

IHS_Thread *IHS_ThreadCreate(IHS_ThreadFunction *function, const char *name, void *context);

void IHS_ThreadJoin(IHS_Thread *thread);

IHS_Mutex *IHS_MutexCreate();

void IHS_MutexDestroy(IHS_Mutex *mutex);

bool IHS_MutexLock(IHS_Mutex *mutex);

bool IHS_MutexUnlock(IHS_Mutex *mutex);

IHS_Cond *IHS_CondCreate();

void IHS_CondDestroy(IHS_Cond *cond);

void IHS_CondSignal(IHS_Cond *cond);

bool IHS_CondWait(IHS_Cond *cond, IHS_Mutex *mutex);
