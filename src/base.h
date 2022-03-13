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
#include <stddef.h>

#include "ihslib/common.h"
#include "ihs_udp.h"
#include "ihs_thread.h"
#include "ihs_queue.h"
#include "ihs_timer.h"

typedef struct IHS_Base IHS_Base;

typedef void (IHS_BaseReceivedFunction)(IHS_Base *base, const IHS_SocketAddress *address, const uint8_t *data,
                                        size_t len);

struct IHS_Base {
    uint64_t deviceId;
    uint8_t secretKey[32];
    char deviceName[64];
    uint8_t deviceToken[32];

    IHS_LogFunction *logFunction;
    IHS_BaseReceivedFunction *receivedCallback;
    IHS_UDPSocket *socket;

    IHS_Thread *worker;
    IHS_Queue *queue;
    IHS_Timers *timers;
    IHS_Mutex *lock;
    bool interrupted;
};

void IHS_BaseInit(IHS_Base *base, const IHS_ClientConfig *config, IHS_BaseReceivedFunction recvCb, bool broadcast);

void IHS_BaseRun(IHS_Base *base);

void IHS_BaseStop(IHS_Base *base);

void IHS_BaseSetLogFunction(IHS_Base *base, IHS_LogFunction *logFunction);

void IHS_BaseLog(IHS_Base *base, IHS_LogLevel level, const char *fmt, ...);

void IHS_BaseThreadedRun(IHS_Base *base);

void IHS_BaseThreadedJoin(IHS_Base *base);

void IHS_BaseFree(IHS_Base *base);

bool IHS_BaseSend(IHS_Base *base, IHS_SocketAddress address, const uint8_t *data, size_t dataLen);

void IHS_BaseLock(IHS_Base *base);

void IHS_BaseUnlock(IHS_Base *base);