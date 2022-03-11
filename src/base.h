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
#include <uv.h>
#include "ihslib/common.h"

typedef struct IHS_Base IHS_Base;

typedef void (IHS_BaseTimerFunction)(IHS_Base *base, void *data);

typedef struct IHS_BaseTimer IHS_BaseTimer;

struct IHS_Base {
    uint64_t deviceId;
    uint8_t secretKey[32];
    char deviceName[64];
    uint8_t deviceToken[32];

    IHS_LogFunction *logFunction;
    uv_loop_t *loop;
    uv_thread_t workerThread;
    uv_udp_t udp;
    uv_mutex_t mutex;
};

void IHS_BaseInit(IHS_Base *base, const IHS_ClientConfig *config, uv_udp_recv_cb recvCb, bool broadcast);

void IHS_BaseRun(IHS_Base *base);

void IHS_BaseStop(IHS_Base *base);

void IHS_BaseSetLogFunction(IHS_Base *base, IHS_LogFunction *logFunction);

void IHS_BaseLog(IHS_Base *base, IHS_LogLevel level, const char *fmt, ...);

void IHS_BaseThreadedRun(IHS_Base *base);

void IHS_BaseThreadedJoin(IHS_Base *base);

void IHS_BaseWaitFinish(IHS_Base *base);

void IHS_BaseFree(IHS_Base *base);

bool IHS_BaseSend(IHS_Base *base, IHS_HostAddress address, const uint8_t *data, size_t dataLen);

void IHS_BaseLock(IHS_Base *base);

void IHS_BaseUnlock(IHS_Base *base);

IHS_BaseTimer *IHS_BaseTimerStart(IHS_Base *base, IHS_BaseTimerFunction timerFn, uint64_t timeout, uint64_t repeat,
                                  void *data);

void IHS_BaseTimerStop(IHS_BaseTimer *timer);