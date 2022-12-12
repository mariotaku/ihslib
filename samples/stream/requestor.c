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

#include <stdio.h>
#include <memory.h>
#include "ihslib.h"
#include "stream.h"
#include "common.h"

typedef struct RequestorCallbacksContext {
    bool requested;
    IHS_SocketAddress address;
    uint8_t sessionKey[64];
    size_t sessionKeyLen;
    bool succeeded;
} RequestorCallbacksContext;

static void OnHostStatus(IHS_Client *client, IHS_HostInfo info, void *context);

static void OnStreamingInProgress(IHS_Client *client, void *context);

static void OnStreamingSuccess(IHS_Client *client, IHS_SocketAddress address, const uint8_t *sessionKey,
                               size_t sessionKeyLen, void *context);

static void OnStreamingFailed(IHS_Client *client, IHS_StreamingResult result, void *context);

bool RequestStream(IHS_SessionInfo *info) {
    IHS_Client *client = IHS_ClientCreate(&clientConfig);
    IHS_ClientDiscoveryCallbacks discoveryCallbacks = {
            .discovered = OnHostStatus,
    };
    IHS_ClientStreamingCallbacks streamingCallbacks = {
            .progress = OnStreamingInProgress,
            .failed = OnStreamingFailed,
            .success = OnStreamingSuccess,
    };
    RequestorCallbacksContext context = {.requested = false, .succeeded = false};
    IHS_ClientSetDiscoveryCallbacks(client, &discoveryCallbacks, &context);
    IHS_ClientSetStreamingCallbacks(client, &streamingCallbacks, &context);
    IHS_ClientSetLogFunction(client, LogPrint);
    bool ret = false;
    if (!IHS_ClientStartDiscovery(client, 0)) {
        fprintf(stderr, "Broadcast error: %s\n", IHS_ClientError(client));
        ret = false;
        goto exit;
    }

    IHS_ClientThreadedJoin(client);

    if (!context.succeeded) {
        ret = false;
        goto exit;
    }
    info->address = context.address;
    info->sessionKeyLen = context.sessionKeyLen;
    memcpy(info->sessionKey, context.sessionKey, context.sessionKeyLen);
    info->steamId = 0;
//    info->steamId = steamId;
    ret = true;

    exit:
    IHS_ClientDestroy(client);
    return ret;
}

static void OnHostStatus(IHS_Client *client, IHS_HostInfo info, void *context) {
    RequestorCallbacksContext *reqContext = context;
    if (reqContext->requested) return;
    reqContext->requested = true;
    IHS_StreamingRequest req = {
            .maxResolution = {1270, 720},
            .streamingEnable = {true, true, true},
            .audioChannelCount = 2,
    };
    if (!IHS_ClientStreamingRequest(client, &info, &req)) {
        fprintf(stderr, "IHS_ClientStreamingRequest failed: %s\n", info.hostname);
        return;
    }
}


void OnStreamingInProgress(IHS_Client *client, void *context) {
    printf("OnStreamingInProgress\n");
}

void OnStreamingSuccess(IHS_Client *client, IHS_SocketAddress address, const uint8_t *sessionKey, size_t sessionKeyLen,
                        void *context) {
    RequestorCallbacksContext *reqContext = context;

    reqContext->address = address;
    reqContext->sessionKeyLen = sessionKeyLen;
    memcpy(reqContext->sessionKey, sessionKey, sessionKeyLen);
    reqContext->succeeded = true;

    IHS_ClientStop(client);
}

void OnStreamingFailed(IHS_Client *client, IHS_StreamingResult result, void *context) {
    printf("OnStreamingFailed(result=%d)\n", result);
    IHS_ClientStop(client);
}