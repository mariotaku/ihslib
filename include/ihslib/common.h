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
#include <stdint.h>

typedef struct IHS_ClientConfig {
    uint64_t deviceId;
    const uint8_t *secretKey;
    const char *deviceName;
} IHS_ClientConfig;

typedef enum IHS_IPAddressFamily {
    IHS_IPAddressFamilyIPv4,
    IHS_IPAddressFamilyIPv6,
} IHS_IPAddressFamily;

typedef union IHS_IPAddress {
    IHS_IPAddressFamily family;
    struct {
        IHS_IPAddressFamily family;
        uint8_t data[4];
    } v4;
    struct {
        IHS_IPAddressFamily family;
        uint8_t data[16];
    } v6;
} IHS_IPAddress;

typedef struct IHS_SocketAddress {
    IHS_IPAddress ip;
    uint16_t port;
} IHS_SocketAddress;

typedef enum IHS_SteamUniverse {
    IHS_SteamUniversePublic = 1,
    IHS_SteamUniverseBeta = 2,
    IHS_SteamUniverseInternal = 3,
    IHS_SteamUniverseDev = 4,
} IHS_SteamUniverse;

typedef struct IHS_HostInfo {
    uint64_t clientId;
    uint64_t instanceId;
    IHS_SocketAddress address;
    char hostname[64];
    IHS_SteamUniverse universe;
    bool gamesRunning;
} IHS_HostInfo;

typedef enum IHS_BaseLogLevel {
    IHS_BaseLogLevelDebug,
    IHS_BaseLogLevelInfo,
    /**
     * Problem that can be self recovered
     */
    IHS_BaseLogLevelWarn,
    /**
     * Error that should stop session
     */
    IHS_BaseLogLevelError,
    /**
     * Irrecoverable error, and the process should be aborted
     */
    IHS_BaseLogLevelFatal,
} IHS_LogLevel;

typedef void (IHS_LogFunction)(IHS_LogLevel level, const char *message);
