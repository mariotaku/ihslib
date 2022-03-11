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

#include "common.h"
#include "audio.h"
#include "video.h"

typedef struct IHS_Session IHS_Session;

typedef struct IHS_SessionConfig {
    IHS_HostAddress address;
    uint8_t sessionKey[32];
    size_t sessionKeyLen;
    uint64_t steamId;
} IHS_SessionConfig;

IHS_Session *IHS_SessionCreate(const IHS_ClientConfig *clientConfig, const IHS_SessionConfig *sessionConfig);

void IHS_SessionSetLogFunction(IHS_Session *session, IHS_LogFunction *logFunction);

void IHS_SessionRun(IHS_Session *session);

void IHS_SessionStop(IHS_Session *session);

void IHS_SessionThreadedRun(IHS_Session *session);

void IHS_SessionThreadedJoin(IHS_Session *session);

void IHS_SessionDestroy(IHS_Session *session);

bool IHS_SessionConnect(IHS_Session *session);

void IHS_SessionDisconnect(IHS_Session *session);

void IHS_SessionSetAudioCallbacks(IHS_Session *session, const IHS_StreamAudioCallbacks *callbacks, void *context);

void IHS_SessionSetVideoCallbacks(IHS_Session *session, const IHS_StreamVideoCallbacks *callbacks, void *context);
