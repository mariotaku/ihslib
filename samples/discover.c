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
#include <signal.h>
#include "ihslib.h"
#include "common.h"

static void OnHostStatus(IHS_Client *client, IHS_HostInfo info, void *context);

static void InterruptHandler(int sig);

static IHS_Client *ActiveClient = NULL;

int main(int argc, char *argv[]) {
    signal(SIGINT, InterruptHandler);
    IHS_ClientConfig config = {deviceId, secretKey, deviceName};
    IHS_Client *client = IHS_ClientCreate(&config);
    IHS_ClientCallbacks callbacks = {
            .hostDiscovered = OnHostStatus
    };
    IHS_ClientSetCallbacks(client, &callbacks, NULL);
    IHS_ClientDiscoveryBroadcast(client);
    ActiveClient = client;
    IHS_ClientRun(client);
    IHS_ClientDestroy(client);
}

static void OnHostStatus(IHS_Client *client, IHS_HostInfo info, void *context) {
    printf("Found device: %s, port: %d, universe: %d\n",
           info.hostname, info.address.port, info.universe);
}

static void InterruptHandler(int sig) {
    if (!ActiveClient) {
        signal(SIGINT, SIG_DFL);
        raise(SIGINT);
        return;
    }
    IHS_ClientStop(ActiveClient);
}