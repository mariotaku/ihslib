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

#pragma once

#include <stdint.h>
#include <stddef.h>

#include "ihslib/common.h"
#include "ihs_udp.h"
#include "ihs_thread.h"
#include "ihs_timer.h"

typedef struct IHS_Base IHS_Base;

typedef void (IHS_BaseReceivedFunction)(IHS_Base *base, const IHS_SocketAddress *address, IHS_Buffer *buffer);

typedef struct IHS_BaseRunCallbacks {
    void (*initialized)(IHS_Base *base, void *context);

    /**
     * Indicating all the resources and states has been destroyed. Nothing can be used beyond this call.
     * @param base
     * @param context
     */
    void (*finalized)(IHS_Base *base, void *context);
} IHS_BaseRunCallbacks;

struct IHS_Base {
    uint64_t deviceId;
    uint8_t secretKey[32];
    char deviceName[64];
    uint8_t deviceToken[32];

    struct {
        IHS_LogFunction *log;
        IHS_BaseReceivedFunction *received;
        const IHS_BaseRunCallbacks *run;
    } callbacks;

    struct {
        void *run;
    } callbackContexts;

    bool broadcast;
    IHS_UDPSocket *socket;

    IHS_Thread *worker;
    IHS_Mutex *lock;
    bool interrupted;
};

#define IHS_UNUSED(x) (void) (x)

void IHS_BaseInit(IHS_Base *base, const IHS_ClientConfig *config, IHS_BaseReceivedFunction recvCb, bool broadcast);

void IHS_BaseSetLogFunction(IHS_Base *base, IHS_LogFunction *logFunction);

void IHS_BaseSetRunCallbacks(IHS_Base *base, const IHS_BaseRunCallbacks *callbacks, void *context);

void IHS_BaseLog(IHS_Base *base, IHS_LogLevel level, const char *tag,
                 const char *fmt, ...) __attribute__ ((format (printf, 4, 5)));

bool IHS_BaseStartWorker(IHS_Base *base, const char *name);

void IHS_BaseInterruptWorker(IHS_Base *base);

void IHS_BaseWaitWorker(IHS_Base *base);

/**
 * Destroys resources allocated in base. The pointer will not be destroyed.
 * @param base Base instance
 */
void IHS_BaseDestroy(IHS_Base *base);

/**
 * Send the data to address immediately.
 * @param base Base instance
 * @param address Target address
 * @param data Data to send
 * @return true if succeeded
 */
bool IHS_BaseSend(IHS_Base *base, IHS_SocketAddress address, const IHS_Buffer *data);

void IHS_BaseLock(IHS_Base *base);

void IHS_BaseUnlock(IHS_Base *base);