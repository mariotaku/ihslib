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
#include <string.h>
#include <unistd.h>
#include "common.h"
#include "ihslib/client.h"
#include "ihslib/session.h"


static void OnHostStatus(IHS_Client *client, IHS_HostInfo info);

void OnStreamingInProgress(IHS_Client *client);

void OnStreamingSuccess(IHS_Client *client, IHS_HostAddress address, const uint8_t *sessionKey, size_t sessionKeyLen);

void OnStreamingFailed(IHS_Client *client, IHS_StreamingResult result);

static bool AuthorizationStart = false;

static IHS_Session *ActiveSession = NULL;

int main(int argc, char *argv[]) {
    IHS_ClientConfig config = {deviceId, secretKey, deviceName};
    IHS_Client *client = IHS_ClientCreate(&config);
    IHS_ClientCallbacks callbacks = {
            .hostDiscovered = OnHostStatus,
            .streamingInProgress = OnStreamingInProgress,
            .streamingFailed = OnStreamingFailed,
            .streamingSuccess = OnStreamingSuccess,
    };
    IHS_ClientSetCallbacks(client, &callbacks);
    IHS_ClientDiscoveryBroadcast(client);
    IHS_ClientDestroy(client);
    if (ActiveSession) {
        IHS_SessionDestroy(ActiveSession);
    }
}

static void OnHostStatus(IHS_Client *client, IHS_HostInfo info) {
    if (AuthorizationStart) return;
    AuthorizationStart = true;
    printf("IHS_ClientStreamingRequest\n");
    IHS_StreamingRequest req = {
            .maxResolution = {1920, 1080},
            .streamingEnable = {true, true, true},
            .audioChannelCount = 2,
            .gamepadCount = 0
    };
    IHS_ClientStreamingRequest(client, &info, &req);
}


void OnStreamingInProgress(IHS_Client *client) {
}

void OnStreamingSuccess(IHS_Client *client, IHS_HostAddress address, const uint8_t *sessionKey, size_t sessionKeyLen) {
    if (ActiveSession) return;
    printf("OnStreamingSuccess(sessionKey=\"");
    for (int i = 0; i < sessionKeyLen; i++) {
        printf("%02x", sessionKey[i]);
    }
    printf("\")\n");
    IHS_ClientStop(client);
    IHS_ClientConfig clientConfig = {deviceId, secretKey, deviceName};
    ActiveSession = IHS_SessionCreate(&clientConfig);
    IHS_SessionConfig sessionConfig;
    sessionConfig.address = address;
    memcpy(sessionConfig.sessionKey, sessionKey, sizeof(sessionConfig.sessionKey));
    IHS_SessionStart(ActiveSession, &sessionConfig);
}

void OnStreamingFailed(IHS_Client *client, IHS_StreamingResult result) {
    printf("OnStreamingFailed(result=%d)\n", result);
    IHS_ClientStop(client);
}