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

#include <malloc.h>
#include <memory.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>

#include "endianness.h"
#include "crypto.h"
#include "ihs_queue.h"

struct IHS_QueueItem {
    enum {
        IHS_BaseQueueSend
    } type;
    union {
        IHS_UDPPacket send;
    };
};

static void QueueItemDestroy(IHS_QueueItem *item);


void IHS_BaseInit(IHS_Base *base, const IHS_ClientConfig *config, IHS_BaseReceivedFunction recvCb, bool broadcast) {
    memset(base, 0, sizeof(IHS_Base));
    base->lock = IHS_MutexCreate();
    base->queue = IHS_QueueCreate(sizeof(IHS_QueueItem), QueueItemDestroy);
    base->timers = IHS_TimersCreate();
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

void IHS_BaseRun(IHS_Base *base) {
    base->socket = IHS_UDPSocketOpen();
    if (base->callbacks.run && base->callbacks.run->initialized) {
        base->callbacks.run->initialized(base, base->callbackContexts.run);
    }
    while (!base->interrupted) {
        IHS_TimersTick(base->timers);
        for (IHS_QueueItem *item; (item = IHS_QueuePoll(base->queue)) != NULL; IHS_QueueItemFree(base->queue, item)) {
            switch (item->type) {
                case IHS_BaseQueueSend:
                    IHS_UDPSocketSend(base->socket, &item->send);
                    break;
                default:
                    IHS_BaseLog(base, IHS_LogLevelFatal, "Queue", "Unrecognized queue item %d", item->type);
                    abort();
            }
        }
        IHS_UDPPacket recv;
        int ret;
        if ((ret = IHS_UDPSocketReceive(base->socket, &recv)) < 0) {
            break;
        }
        if (ret) {
            base->callbacks.received(base, &recv.address, recv.buffer, recv.length);
        }
    }
    if (base->callbacks.run && base->callbacks.run->finalized) {
        base->callbacks.run->finalized(base, base->callbackContexts.run);
    }
    IHS_UDPSocketClose(base->socket);
}

void IHS_BaseStop(IHS_Base *base) {
    if (base->interrupted) {
        return;
    }
    base->interrupted = true;
    IHS_UDPSocketUnblock(base->socket);
}

void IHS_BaseSetLogFunction(IHS_Base *base, IHS_LogFunction *logFunction) {
    IHS_BaseLock(base);
    base->callbacks.log = logFunction;
    IHS_BaseUnlock(base);
}

void IHS_BaseSetRunCallbacks(IHS_Base *base, const IHS_BaseRunCallbacks *callbacks, void *context) {
    IHS_BaseLock(base);
    base->callbacks.run = callbacks;
    base->callbackContexts.run = context;
    IHS_BaseUnlock(base);
}

void IHS_BaseLog(IHS_Base *base, IHS_LogLevel level, const char *tag, const char *fmt, ...) {
    if (!base->callbacks.log) return;
    char buf[4096];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, 4095, fmt, args);
    base->callbacks.log(level, tag, buf);
    va_end(args);
}

void IHS_BaseStartWorker(IHS_Base *base, const char *name, IHS_ThreadFunction *worker) {
    IHS_BaseLock(base);
    if (!base->worker) {
        base->worker = IHS_ThreadCreate(worker, name, base);
    }
    IHS_BaseUnlock(base);
}

void IHS_BaseThreadedJoin(IHS_Base *base) {
    IHS_ThreadJoin(base->worker);
    base->worker = NULL;
}

void IHS_BaseFree(IHS_Base *base) {
    IHS_TimersDestroy(base->timers);
    IHS_QueueDestroy(base->queue);
    IHS_MutexDestroy(base->lock);
}

bool IHS_BaseSend(IHS_Base *base, IHS_SocketAddress address, const uint8_t *data, size_t dataLen) {
    IHS_QueueItem *item = IHS_QueueItemObtain(base->queue);
    item->type = IHS_BaseQueueSend;
    item->send.address = address;
    item->send.length = dataLen;
    item->send.buffer = malloc(dataLen);
    memcpy(item->send.buffer, data, dataLen);

    IHS_QueueAppend(base->queue, item);
    if (base->socket != NULL) {
        IHS_UDPSocketUnblock(base->socket);
    }
    return true;
}

void IHS_BaseLock(IHS_Base *base) {
    IHS_MutexLock(base->lock);
}

void IHS_BaseUnlock(IHS_Base *base) {
    IHS_MutexUnlock(base->lock);
}

static void QueueItemDestroy(IHS_QueueItem *item) {
    switch (item->type) {
        case IHS_BaseQueueSend:
            free(item->send.buffer);
            break;
    }
}