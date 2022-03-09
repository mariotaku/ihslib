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

#pragma once

#include <stdbool.h>

#if __WIN32__
#include <in6addr.h>
#include <winsock2.h>
#else

#include <sys/socket.h>
#include <netinet/in.h>

#endif

typedef struct IHS_ClientConfig {
    uint64_t deviceId;
    const uint8_t *secretKey;
    const char *deviceName;
} IHS_ClientConfig;

typedef struct IHS_HostIP {
    enum {
        IHS_HostIPv4 = AF_INET,
        IHS_HostIPv6 = AF_INET6,
    } type;
    union {
        struct in_addr v4;
        struct in6_addr v6;
    } value;
} IHS_HostIP;

typedef struct IHS_HostAddress {
    IHS_HostIP ip;
    uint16_t port;
} IHS_HostAddress;

typedef enum IHS_SteamUniverse {
    IHS_SteamUniversePublic = 1,
    IHS_SteamUniverseBeta = 2,
    IHS_SteamUniverseInternal = 3,
    IHS_SteamUniverseDev = 4,
} IHS_SteamUniverse;

typedef struct IHS_HostInfo {
    uint64_t clientId;
    uint64_t instanceId;
    IHS_HostAddress address;
    char hostname[64];
    IHS_SteamUniverse universe;
    bool gamesRunning;
} IHS_HostInfo;