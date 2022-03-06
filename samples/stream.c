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
#include <signal.h>

#include "ihslib.h"
#include "common.h"


static void OnHostStatus(IHS_Client *client, IHS_HostInfo info);

static void OnStreamingInProgress(IHS_Client *client);

static void OnStreamingSuccess(IHS_Client *client, IHS_HostAddress address, const uint8_t *sessionKey,
                               size_t sessionKeyLen);

static void OnStreamingFailed(IHS_Client *client, IHS_StreamingResult result);

static void InterruptHandler(int sig);

static void OnAudioStart(void *context, const IHS_StreamAudioConfig *config);

static void OnAudioReceived(void *context, const uint8_t *data, size_t dataLen);

static void OnAudioStop(void *context);

static void OnVideoStart(void *context, const IHS_StreamVideoConfig *config);

static void OnVideoReceived(void *context, const uint8_t *data, size_t dataLen);

static void OnVideoStop(void *context);

static bool AuthorizationStart = false;
static bool Running = true;

static IHS_Session *ActiveSession = NULL;

static IHS_StreamAudioCallbacks AudioCallbacks = {
        .start = OnAudioStart,
        .received = OnAudioReceived,
        .stop = OnAudioStop,
};

static IHS_StreamVideoCallbacks VideoCallbacks = {
        .start = OnVideoStart,
        .received = OnVideoReceived,
        .stop = OnVideoStop,
};

int main(int argc, char *argv[]) {
    signal(SIGINT, InterruptHandler);

    IHS_ClientConfig config = {deviceId, secretKey, deviceName};
    IHS_Client *client = IHS_ClientCreate(&config);
    IHS_ClientCallbacks callbacks = {
            .hostDiscovered = OnHostStatus,
            .streamingInProgress = OnStreamingInProgress,
            .streamingFailed = OnStreamingFailed,
            .streamingSuccess = OnStreamingSuccess,
    };
    IHS_ClientSetCallbacks(client, &callbacks);
    printf("IHS_ClientDiscoveryBroadcast\n");
    if (!IHS_ClientDiscoveryBroadcast(client)) {
        fprintf(stderr, "Broadcast error: %s\n", IHS_ClientError(client));
    }

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
    printf("OnStreamingSuccess(sessionKey[%u]=\"", sessionKeyLen);
    for (int i = 0; i < sessionKeyLen; i++) {
        printf("%02x", sessionKey[i]);
    }
    printf("\")\n");
    IHS_ClientStop(client);
    IHS_ClientConfig clientConfig = {deviceId, secretKey, deviceName};
    ActiveSession = IHS_SessionCreate(&clientConfig);
    IHS_SessionSetAudioCallbacks(ActiveSession, &AudioCallbacks, NULL);
    IHS_SessionSetVideoCallbacks(ActiveSession, &VideoCallbacks, NULL);
    IHS_SessionConfig sessionConfig;
    sessionConfig.address = address;
    memcpy(sessionConfig.sessionKey, sessionKey, sessionKeyLen);
    sessionConfig.sessionKeyLen = sessionKeyLen;
    IHS_SessionStart(ActiveSession, &sessionConfig);
}

void OnStreamingFailed(IHS_Client *client, IHS_StreamingResult result) {
    printf("OnStreamingFailed(result=%d)\n", result);
    IHS_ClientStop(client);
}

static void InterruptHandler(int sig) {
    if (!ActiveSession) {
        signal(SIGINT, SIG_DFL);
        raise(SIGINT);
        return;
    }
    IHS_SessionDisconnect(ActiveSession);
}

static void OnAudioStart(void *context, const IHS_StreamAudioConfig *config) {
    printf("OnAudioStart\n");
}

static void OnAudioReceived(void *context, const uint8_t *data, size_t dataLen) {
    printf("OnAudioReceived\n");
}

static void OnAudioStop(void *context) {
    printf("OnAudioStop\n");
}

static void OnVideoStart(void *context, const IHS_StreamVideoConfig *config) {
    printf("OnVideoStart\n");
}

static void OnVideoReceived(void *context, const uint8_t *data, size_t dataLen) {
    printf("OnVideoReceived\n");
}

static void OnVideoStop(void *context) {
    printf("OnVideoStop\n");
}