#pragma once


#include <stdint.h>
#include <uv.h>
#include "ihslib/common.h"

typedef struct IHS_Base {
    uint64_t deviceId;
    uint8_t secretKey[32];
    char deviceName[64];
    uint8_t deviceToken[32];

    uv_loop_t *loop;
    uv_thread_t workerThread;
    uv_udp_t udp;
    uv_mutex_t mutex;
} IHS_Base;

void IHS_BaseInit(IHS_Base *base, const IHS_ClientConfig *config, uv_udp_recv_cb recvCb, bool broadcast);

void IHS_BaseStop(IHS_Base *base);

void IHS_BaseFree(IHS_Base *base);

void IHS_BaseSend(IHS_Base *base, IHS_HostAddress address,const uint8_t *data, size_t dataLen);

void IHS_BaseLock(IHS_Base *base);

void IHS_BaseUnlock(IHS_Base *base);

