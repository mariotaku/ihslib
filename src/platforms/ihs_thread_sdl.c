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
#include "ihs_thread.h"

#include <SDL.h>

IHS_Thread *IHS_ThreadCreate(IHS_ThreadFunction *function, const char *name, void *context) {
    return (IHS_Thread *) SDL_CreateThread((SDL_ThreadFunction) function, name, context);
}

void IHS_ThreadJoin(IHS_Thread *thread) {
    SDL_assert(thread != NULL);
    SDL_WaitThread((SDL_Thread *) thread, NULL);
}

IHS_Mutex *IHS_MutexCreate() {
    return (IHS_Mutex *) SDL_CreateMutex();
}

void IHS_MutexDestroy(IHS_Mutex *mutex) {
    SDL_assert(mutex != NULL);
    return SDL_DestroyMutex((SDL_mutex *) mutex);
}

bool IHS_MutexLock(IHS_Mutex *mutex) {
    SDL_assert(mutex != NULL);
    return SDL_LockMutex((SDL_mutex *) mutex) == 0;
}

bool IHS_MutexUnlock(IHS_Mutex *mutex) {
    SDL_assert(mutex != NULL);
    return SDL_UnlockMutex((SDL_mutex *) mutex) == 0;
}

IHS_Cond *IHS_CondCreate() {
    return (IHS_Cond *) SDL_CreateCond();
}

void IHS_CondDestroy(IHS_Cond *cond) {
    SDL_assert(cond != NULL);
    SDL_DestroyCond((SDL_cond *) cond);
}

void IHS_CondSignal(IHS_Cond *cond) {
    SDL_assert(cond != NULL);
    SDL_CondSignal((SDL_cond *) cond);
}

bool IHS_CondWait(IHS_Cond *cond, IHS_Mutex *mutex) {
    SDL_assert(cond != NULL);
    return SDL_CondWait((SDL_cond *) cond, (SDL_mutex *) mutex) == 0;
}
