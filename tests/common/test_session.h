/*
 *  _____  _   _  _____  _  _  _     
 * |_   _|| | | |/  ___|| |(_)| |     Steam    
 *   | |  | |_| |\ `--. | | _ | |__     In-Home
 *   | |  |  _  | `--. \| || || '_ \      Streaming
 *  _| |_ | | | |/\__/ /| || || |_) |       Library
 *  \___/ \_| |_/\____/ |_||_||_.__/
 *
 * Copyright (c) 2022 Mariotaku <https://github.com/mariotaku>.
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

#include "session/session_pri.h"


static const uint64_t deviceId = 11451419190810;

static const uint8_t secretKey[32] = {
        11, 45, 14, 19, 19, 8, 1, 0,
        11, 45, 14, 19, 19, 8, 1, 0,
        11, 45, 14, 19, 19, 8, 1, 0,
        11, 45, 14, 19, 19, 8, 1, 0,
};

static const char deviceName[] = "BABYLON STAGE34\0";

static const IHS_ClientConfig clientConfig = {deviceId, secretKey, deviceName};

static const IHS_SessionInfo sessionInfo = {
        .address = {
                .port = 11451,
                .ip.v4 = {IHS_IPAddressFamilyIPv4, {127, 0, 0, 1}}
        },
        .sessionKey = {
                8, 1, 0, 11, 45, 14, 19, 19,
                8, 1, 0, 11, 45, 14, 19, 19,
                8, 1, 0, 11, 45, 14, 19, 19,
                8, 1, 0, 11, 45, 14, 19, 19,
        },
        .sessionKeyLen = 32,
        .steamId = 0,
};

IHS_Session *IHS_TestSessionCreate();