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

#include <string.h>
#include <malloc.h>

#include "endianness.h"
#include "ihslib/crypto.h"

static void BaseThread(IHS_Base *base);

static void SendCallback(uv_udp_send_t *req, int status);

static uv_buf_t BufferAlloc(uv_handle_t *handle, size_t suggested_size);

void IHS_BaseInit(IHS_Base *base, const IHS_ClientConfig *config, uv_udp_recv_cb recvCb, bool broadcast) {

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
    uv_udp_bind(&base->udp, uv_ip4_addr("0.0.0.0", 0), 0);

    uv_udp_set_broadcast(&base->udp, broadcast);
    uv_udp_recv_start(&base->udp, BufferAlloc, recvCb);
    uv_thread_create(&base->workerThread, (void (*)(void *)) BaseThread, base);
}

void IHS_BaseStop(IHS_Base *base) {
    uv_stop(base->loop);
}

void IHS_BaseFree(IHS_Base *base) {
    uv_thread_join(&base->workerThread);
    uv_mutex_destroy(&base->mutex);
    uv_loop_delete(base->loop);
}

void IHS_BaseSend(IHS_Base *base, IHS_HostAddress address, const uint8_t *data, size_t dataLen) {
    uv_buf_t uvbuf = uv_buf_init(malloc(dataLen), dataLen);
    memcpy(uvbuf.base, data, dataLen);
    uv_udp_send_t *req = malloc(sizeof(uv_udp_send_t));
    if (address.ip.type == IHS_HostIPv4) {
        struct sockaddr_in send_addr = {AF_INET, htons(address.port), .sin_addr= address.ip.value.v4};
        uv_udp_send(req, &base->udp, &uvbuf, 1, send_addr, SendCallback);
    } else if (address.ip.type == IHS_HostIPv6) {
        struct sockaddr_in6 send_addr = {AF_INET6, htons(address.port), .sin6_addr = address.ip.value.v6};
        uv_udp_send6(req, &base->udp, &uvbuf, 1, send_addr, SendCallback);
    }
}

void IHS_BaseLock(IHS_Base *base) {
    uv_mutex_lock(&base->mutex);
}

void IHS_BaseUnlock(IHS_Base *base) {
    uv_mutex_unlock(&base->mutex);
}

static uv_buf_t BufferAlloc(uv_handle_t *handle, size_t suggested_size) {
    (void) handle;
    return uv_buf_init(malloc(suggested_size), suggested_size);
}

static void BaseThread(IHS_Base *base) {
    uv_run(base->loop, UV_RUN_DEFAULT);
}

static void SendCallback(uv_udp_send_t *req, int status) {
    if (status != 0) {
        printf("Error: %s\n", strerror(status));
    }
    free(req->bufsml[0].base);
    free(req);
}