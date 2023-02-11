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

#include "base.h"

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <assert.h>

#include "endianness.h"
#include "crypto.h"
#include "ihs_buffer.h"

static void BaseWorker(IHS_Base *base);

static bool initialized;

void IHS_Init() {
    initialized = true;
    IHS_TimerInit();
}

void IHS_Quit() {
    IHS_TimerQuit();
    initialized = false;
}

void IHS_BaseInit(IHS_Base *base, const IHS_ClientConfig *config, IHS_BaseReceivedFunction recvCb, bool broadcast) {
    assert(base != NULL);
    assert(initialized);
    memset(base, 0, sizeof(IHS_Base));
    base->broadcast = broadcast;
    base->lock = IHS_MutexCreate();
    base->callbacks.received = recvCb;

    base->deviceId = config->deviceId;
    memcpy(base->secretKey, config->secretKey, 32);
    strncpy(base->deviceName, config->deviceName ? config->deviceName : "IHSLib", sizeof(base->deviceName));

    uint8_t in[8];
    size_t deviceTokenLen = sizeof(base->deviceToken);
    IHS_WriteUInt64LE(in, base->deviceId);
    IHS_CryptoSymmetricEncrypt(in, 8, base->secretKey, sizeof(base->secretKey),
                               base->deviceToken, &deviceTokenLen);
}

void IHS_BaseSetLogFunction(IHS_Base *base, IHS_LogFunction *logFunction) {
    assert(base != NULL);
    IHS_BaseLock(base);
    base->callbacks.log = logFunction;
    IHS_BaseUnlock(base);
}

void IHS_BaseSetRunCallbacks(IHS_Base *base, const IHS_BaseRunCallbacks *callbacks, void *context) {
    assert(base != NULL);
    IHS_BaseLock(base);
    base->callbacks.run = callbacks;
    base->callbackContexts.run = context;
    IHS_BaseUnlock(base);
}

void IHS_BaseLog(IHS_Base *base, IHS_LogLevel level, const char *tag, const char *fmt, ...) {
    assert(base != NULL);
    if (!base->callbacks.log) return;
    char buf[4096];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, 4095, fmt, args);
    base->callbacks.log(level, tag, buf);
    va_end(args);
}

const char *IHS_LogLevelName(IHS_LogLevel level) {
    switch (level) {
        case IHS_LogLevelFatal:
            return "Fatal";
        case IHS_LogLevelError:
            return "Error";
        case IHS_LogLevelWarn:
            return "Warn";
        case IHS_LogLevelInfo:
            return "Info";
        case IHS_LogLevelDebug:
            return "Debug";
        case IHS_LogLevelVerbose:
            return "Verbose";
        default:
            return "";
    }
}

bool IHS_BaseStartWorker(IHS_Base *base, const char *name) {
    assert(base != NULL);
    IHS_BaseLock(base);
    if (base->worker != NULL) {
        IHS_BaseUnlock(base);
        return false;
    }
    base->worker = IHS_ThreadCreate((IHS_ThreadFunction *) BaseWorker, name, base);
    IHS_BaseLog(base, IHS_LogLevelInfo, "Worker", "Worker thread %s created", name);
    IHS_BaseUnlock(base);
    return true;
}

void IHS_BaseInterruptWorker(IHS_Base *base) {
    assert(base != NULL);
    if (base->interrupted) {
        return;
    }
    base->interrupted = true;
}

void IHS_BaseWaitWorker(IHS_Base *base) {
    assert(base != NULL);
    IHS_ThreadJoin(base->worker);
    base->worker = NULL;
}

void IHS_BaseDestroy(IHS_Base *base) {
    assert(base != NULL);
    IHS_MutexDestroy(base->lock);
}

bool IHS_BaseSend(IHS_Base *base, IHS_SocketAddress address, const IHS_Buffer *data) {
    assert(base != NULL);
    if (base->socket == NULL) {
        return false;
    }
    IHS_UDPPacket packet = {.address = address, .buffer = *data};
    return IHS_UDPSocketSend(base->socket, &packet);
}

void IHS_BaseLock(IHS_Base *base) {
    assert(base != NULL);
    IHS_MutexLock(base->lock);
}

void IHS_BaseUnlock(IHS_Base *base) {
    assert(base != NULL);
    IHS_MutexUnlock(base->lock);
}

static void BaseWorker(IHS_Base *base) {
    assert(base != NULL);
    base->socket = IHS_UDPSocketOpen(base->broadcast);
    if (base->callbacks.run && base->callbacks.run->initialized) {
        base->callbacks.run->initialized(base, base->callbackContexts.run);
    }
    IHS_UDPPacket recv;
    IHS_BufferInit(&recv.buffer, 2048, 2048);
    while (!base->interrupted) {
        int ret;
        if ((ret = IHS_UDPSocketReceive(base->socket, &recv)) < 0) {
            break;
        }
        if (ret) {
            base->callbacks.received(base, &recv.address, &recv.buffer);
        }
        IHS_BufferClear(&recv.buffer, false);
    }
    IHS_BufferClear(&recv.buffer, true);
    if (base->callbacks.run && base->callbacks.run->finalized) {
        base->callbacks.run->finalized(base, base->callbackContexts.run);
    }
    IHS_UDPSocketClose(base->socket);
}

