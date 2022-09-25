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

static void Log(IHS_LogLevel level, const char *tag, const char *message);

int main(int argc, char *argv[]) {
    signal(SIGINT, InterruptHandler);
    IHS_ClientConfig config = {deviceId, secretKey, deviceName};
    IHS_Client *client = IHS_ClientCreate(&config);
    ActiveClient = client;
    IHS_ClientSetLogFunction(client, Log);
    IHS_ClientDiscoveryCallbacks callbacks = {
            .discovered = OnHostStatus
    };
    IHS_ClientSetDiscoveryCallbacks(client, &callbacks, NULL);
    IHS_ClientStartDiscovery(client, 10000);
    IHS_ClientThreadedJoin(client);
    IHS_ClientDestroy(client);
}

static void OnHostStatus(IHS_Client *client, IHS_HostInfo info, void *context) {
    printf("[Sample] Found device: %s, port: %d, universe: %d\n", info.hostname, info.address.port,
           info.universe);
}

static void InterruptHandler(int sig) {
    if (ActiveClient == NULL) {
        signal(SIGINT, SIG_DFL);
        raise(SIGINT);
        return;
    }
    IHS_ClientStopDiscovery(ActiveClient);
    IHS_ClientStop(ActiveClient);
}

static void Log(IHS_LogLevel level, const char *tag, const char *message) {
    switch (level) {
        case IHS_LogLevelInfo:
            fprintf(stderr, "[%s]\x1b[36m %s\x1b[0m\n", tag, message);
            break;
        case IHS_LogLevelWarn:
            fprintf(stderr, "[%s]\x1b[33m %s\x1b[0m\n", tag, message);
            break;
        case IHS_LogLevelError:
            fprintf(stderr, "[%s]\x1b[31m %s\x1b[0m\n", tag, message);
            break;
        case IHS_LogLevelFatal:
            fprintf(stderr, "[%s]\x1b[41m %s\x1b[0m\n", tag, message);
            break;
        default:
            fprintf(stderr, "[%s] %s\n", tag, message);
            break;
    }
}