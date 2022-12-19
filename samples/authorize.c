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
#include "common.h"
#include "ihslib/client.h"


static void OnHostStatus(IHS_Client *client, const IHS_HostInfo *info, void *context);

void OnAuthorizationInProgress(IHS_Client *client, const IHS_HostInfo *info, void *context);

void OnAuthorizationSuccess(IHS_Client *client, const IHS_HostInfo *info, uint64_t steamId, void *context);

void OnAuthorizationFailed(IHS_Client *client, const IHS_HostInfo *info, IHS_AuthorizationResult result, void *context);

static bool AuthorizationStart = false;


int main(int argc, char *argv[]) {
    IHS_Init();
    IHS_ClientConfig config = {deviceId, secretKey, deviceName};
    IHS_Client *client = IHS_ClientCreate(&config);
    IHS_ClientDiscoveryCallbacks dcallbacks = {
            .discovered = OnHostStatus,
    };
    IHS_ClientAuthorizationCallbacks acallbacks = {
            .progress = OnAuthorizationInProgress,
            .failed = OnAuthorizationFailed,
            .success = OnAuthorizationSuccess,
    };
    IHS_ClientSetDiscoveryCallbacks(client, &dcallbacks, NULL);
    IHS_ClientSetAuthorizationCallbacks(client, &acallbacks, NULL);
    IHS_ClientStartDiscovery(client, 0);
    IHS_ClientThreadedJoin(client);
    IHS_ClientDestroy(client);
    IHS_Quit();
}

static void OnHostStatus(IHS_Client *client, const IHS_HostInfo *info, void *context) {
    if (AuthorizationStart) return;
    AuthorizationStart = true;
    printf("IHS_ClientAuthorizationRequest");
    IHS_ClientAuthorizationRequest(client, info, "1919");
}


void OnAuthorizationInProgress(IHS_Client *client, const IHS_HostInfo *info, void *context) {
}

void OnAuthorizationSuccess(IHS_Client *client, const IHS_HostInfo *info, uint64_t steamId, void *context) {
    printf("OnStreamingSuccess(steamId=%llu)\n", steamId);
    IHS_ClientStop(client);

}

void
OnAuthorizationFailed(IHS_Client *client, const IHS_HostInfo *info, IHS_AuthorizationResult result, void *context) {
    printf("OnStreamingFailed(result=%d)\n", result);
    IHS_ClientStop(client);
}