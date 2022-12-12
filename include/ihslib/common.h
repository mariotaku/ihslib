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

#include "net.h"

typedef struct IHS_ClientConfig {
    uint64_t deviceId;
    const uint8_t *secretKey;
    const char *deviceName;
} IHS_ClientConfig;

typedef enum IHS_SteamUniverse {
    IHS_SteamUniversePublic = 1,
    IHS_SteamUniverseBeta = 2,
    IHS_SteamUniverseInternal = 3,
    IHS_SteamUniverseDev = 4,
} IHS_SteamUniverse;

/**
 * @see https://github.com/SteamDatabase/SteamTracking/blob/master/Structs/EOSType.h
 */
typedef enum IHS_SteamOSType {
    IHS_SteamOSTypeWeb = -700,
    IHS_SteamOSTypeIos = -600,
    IHS_SteamOSTypeAndroid = -500,
    IHS_SteamOSTypeAndroid6 = -499,
    IHS_SteamOSTypeAndroid7 = -498,
    IHS_SteamOSTypeAndroid8 = -497,
    IHS_SteamOSTypeAndroid9 = -496,
    IHS_SteamOSTypePs3os = -300,
    IHS_SteamOSTypeLinux = -203,
    IHS_SteamOSTypeLinux22 = -202,
    IHS_SteamOSTypeLinux24 = -201,
    IHS_SteamOSTypeLinux26 = -200,
    IHS_SteamOSTypeLinux32 = -199,
    IHS_SteamOSTypeLinux35 = -198,
    IHS_SteamOSTypeLinux36 = -197,
    IHS_SteamOSTypeLinux310 = -196,
    IHS_SteamOSTypeLinux316 = -195,
    IHS_SteamOSTypeLinux318 = -194,
    IHS_SteamOSTypeLinux3x = -193,
    IHS_SteamOSTypeLinux4x = -192,
    IHS_SteamOSTypeLinux41 = -191,
    IHS_SteamOSTypeLinux44 = -190,
    IHS_SteamOSTypeLinux49 = -189,
    IHS_SteamOSTypeLinux414 = -188,
    IHS_SteamOSTypeLinux419 = -187,
    IHS_SteamOSTypeLinux5x = -186,
    IHS_SteamOSTypeLinux54 = -185,
    IHS_SteamOSTypeLinux6x = -184,
    IHS_SteamOSTypeLinux7x = -183,
    IHS_SteamOSTypeLinux510 = -182,
    IHS_SteamOSTypeMacos = -102,
    IHS_SteamOSTypeMacos104 = -101,
    IHS_SteamOSTypeMacos105 = -100,
    IHS_SteamOSTypeMacos1058 = -99,
    IHS_SteamOSTypeMacos106_unused1 = -98,
    IHS_SteamOSTypeMacos106_unused2 = -97,
    IHS_SteamOSTypeMacos106_unused3 = -96,
    IHS_SteamOSTypeMacos106 = -95,
    IHS_SteamOSTypeMacos1063 = -94,
    IHS_SteamOSTypeMacos1064_slgu = -93,
    IHS_SteamOSTypeMacos1067 = -92,
    IHS_SteamOSTypeMacos1067_unused = -91,
    IHS_SteamOSTypeMacos107 = -90,
    IHS_SteamOSTypeMacos108 = -89,
    IHS_SteamOSTypeMacos109 = -88,
    IHS_SteamOSTypeMacos1010 = -87,
    IHS_SteamOSTypeMacos1011 = -86,
    IHS_SteamOSTypeMacos1012 = -85,
    IHS_SteamOSTypeMacos1013 = -84,
    IHS_SteamOSTypeMacos1014 = -83,
    IHS_SteamOSTypeMacos1015 = -82,
    IHS_SteamOSTypeMacos1016 = -81,
    IHS_SteamOSTypeMacos11 = -80,
    IHS_SteamOSTypeMacos111 = -79,
    IHS_SteamOSTypeMacos1017 = -78,
    IHS_SteamOSTypeMacos12 = -77,
    IHS_SteamOSTypeMacos13 = -75,
    IHS_SteamOSTypeMacos14 = -74,
    IHS_SteamOSTypeMacos15 = -73,
    IHS_SteamOSTypeUnknown = -1,
    IHS_SteamOSTypeWindows = 0,
    IHS_SteamOSTypeWin311 = 1,
    IHS_SteamOSTypeWin95 = 2,
    IHS_SteamOSTypeWin98 = 3,
    IHS_SteamOSTypeWinME = 4,
    IHS_SteamOSTypeWinNT = 5,
    IHS_SteamOSTypeWin200 = 6,
    IHS_SteamOSTypeWinXP = 7,
    IHS_SteamOSTypeWin2003 = 8,
    IHS_SteamOSTypeWinVista = 9,
    IHS_SteamOSTypeWin7 = 10,
    IHS_SteamOSTypeWin2008 = 11,
    IHS_SteamOSTypeWin2012 = 12,
    IHS_SteamOSTypeWin8 = 13,
    IHS_SteamOSTypeWin81 = 14,
    IHS_SteamOSTypeWin2012R2 = 15,
    IHS_SteamOSTypeWin10 = 16,
    IHS_SteamOSTypeWin2016 = 17,
    IHS_SteamOSTypeWin2019 = 18,
    IHS_SteamOSTypeWin2022 = 19,
    IHS_SteamOSTypeWin11 = 20,
} IHS_SteamOSType;

typedef struct IHS_HostInfo {
    uint64_t clientId;
    uint64_t instanceId;
    IHS_SocketAddress address;
    char hostname[64];
    IHS_SteamOSType ostype;
    IHS_SteamUniverse universe;
    bool gamesRunning;
} IHS_HostInfo;

typedef enum IHS_LogLevel {
    /**
     * Irrecoverable error, and the process should be aborted
     */
    IHS_LogLevelFatal,
    /**
     * Error that should stop session
     */
    IHS_LogLevelError,
    /**
     * Problem that can be self-recovered
     */
    IHS_LogLevelWarn,
    /**
     * Informative message
     */
    IHS_LogLevelInfo,
    IHS_LogLevelDebug,
    IHS_LogLevelVerbose,
} IHS_LogLevel;

typedef void (IHS_LogFunction)(IHS_LogLevel level, const char *tag, const char *message);

void IHS_Init();

void IHS_Quit();

const char *IHS_LogLevelName(IHS_LogLevel level);
