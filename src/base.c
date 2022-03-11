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

#include "endianness.h"
#include "crypto.h"

static void SendCallback(uv_udp_send_t *req, int status);

static uv_buf_t BufferAlloc(uv_handle_t *handle, size_t size);

static void BaseTimer(uv_timer_t *handle, int status);

static void BaseTimerCleanup(uv_handle_t *handle);

struct IHS_BaseTimer {
    IHS_BaseTimerFunction *fn;
    uv_timer_t *uv;
    void *data;
};

void IHS_BaseInit(IHS_Base *base, const IHS_ClientConfig *config, uv_udp_recv_cb recvCb, bool broadcast) {
    memset(base, 0, sizeof(IHS_Base));
    base->deviceId = config->deviceId;
    memcpy(base->secretKey, config->secretKey, 32);
    strncpy(base->deviceName, config->deviceName ? config->deviceName : "IHSLib", sizeof(base->deviceName));

    uint8_t in[8];
    size_t deviceTokenLen = sizeof(base->deviceToken);
    IHS_WriteUInt64LE(in, base->deviceId);
    IHS_CryptoSymmetricEncrypt(in, 8, base->secretKey, sizeof(base->secretKey),
                               base->deviceToken, &deviceTokenLen);

    base->loop = uv_loop_new();
    uv_mutex_init(&base->mutex);
    base->loop->data = base;
    uv_udp_init(base->loop, &base->udp);
    struct sockaddr_in listenAddr = {AF_INET, 0, INADDR_ANY};
    uv_udp_bind(&base->udp, listenAddr, 0);

    uv_udp_set_broadcast(&base->udp, broadcast);
    uv_udp_recv_start(&base->udp, BufferAlloc, recvCb);
}

void IHS_BaseRun(IHS_Base *base) {
    uv_run(base->loop, UV_RUN_DEFAULT);
}

void IHS_BaseStop(IHS_Base *base) {
    uv_stop(base->loop);
}

void IHS_BaseSetLogFunction(IHS_Base *base, IHS_BaseLogFunction *logFunction) {
    base->logFunction = logFunction;
}

void IHS_BaseLog(IHS_Base *base, IHS_BaseLogLevel level, const char *fmt, ...) {
    if (!base->logFunction) return;
    char buf[4096];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, 4095, fmt, args);
    base->logFunction(level, fmt);
    va_end(args);
}

void IHS_BaseThreadedRun(IHS_Base *base) {
    uv_thread_create(&base->workerThread, (void (*)(void *)) IHS_BaseRun, base);
}

void IHS_BaseThreadedJoin(IHS_Base *base) {
    uv_thread_join(&base->workerThread);
}

void IHS_BaseFree(IHS_Base *base) {
    uv_mutex_destroy(&base->mutex);
    uv_loop_delete(base->loop);
}

bool IHS_BaseSend(IHS_Base *base, IHS_HostAddress address, const uint8_t *data, size_t dataLen) {
    uv_buf_t uvbuf = uv_buf_init(malloc(dataLen), dataLen);
    memcpy(uvbuf.base, data, dataLen);
    uv_udp_send_t *req = malloc(sizeof(uv_udp_send_t));
    if (address.ip.type == IHS_HostIPv4) {
        struct sockaddr_in send_addr = {AF_INET, htons(address.port), .sin_addr= address.ip.value.v4};
        return uv_udp_send(req, &base->udp, &uvbuf, 1, send_addr, SendCallback) == 0;
    } else if (address.ip.type == IHS_HostIPv6) {
        struct sockaddr_in6 send_addr = {AF_INET6, htons(address.port), .sin6_addr = address.ip.value.v6};
        return uv_udp_send6(req, &base->udp, &uvbuf, 1, send_addr, SendCallback) == 0;
    }
    return false;
}

void IHS_BaseLock(IHS_Base *base) {
    uv_mutex_lock(&base->mutex);
}

void IHS_BaseUnlock(IHS_Base *base) {
    uv_mutex_unlock(&base->mutex);
}

IHS_BaseTimer *IHS_BaseTimerStart(IHS_Base *base, IHS_BaseTimerFunction timerFn, uint64_t timeout, uint64_t repeat,
                                  void *data) {
    IHS_BaseTimer *timer = malloc(sizeof(IHS_BaseTimer));
    timer->fn = timerFn;
    timer->uv = malloc(sizeof(uv_timer_t));
    timer->data = data;
    uv_timer_init(base->loop, timer->uv);
    timer->uv->data = timer;
    timer->uv->close_cb = BaseTimerCleanup;
    uv_timer_start(timer->uv, BaseTimer, timeout, repeat);
    return timer;
}

void IHS_BaseTimerStop(IHS_BaseTimer *timer) {
    uv_timer_stop(timer->uv);
}

static uv_buf_t BufferAlloc(uv_handle_t *handle, size_t size) {
    (void) handle;
    char *buf = malloc(size);
    if (buf == NULL) {
        IHS_Base *base = handle->loop->data;
        IHS_BaseLog(base, IHS_BaseLogLevelFatal, "Failed to allocate %u bytes of buffer!!!", size);
        abort();
    }
    return uv_buf_init(buf, size);
}

static void SendCallback(uv_udp_send_t *req, int status) {
    if (status != 0) {
        IHS_Base *base = req->handle->loop->data;
        IHS_BaseLog(base, IHS_BaseLogLevelError, "Error: %s", strerror(status));
    }
    free(req->bufsml[0].base);
    free(req);
}

static void BaseTimer(uv_timer_t *handle, int status) {
    IHS_BaseTimer *timer = handle->data;
    timer->fn(handle->loop->data, timer->data);
}

static void BaseTimerCleanup(uv_handle_t *handle) {
    free(handle->data);
    free(handle);
}