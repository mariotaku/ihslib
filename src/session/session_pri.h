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

#include "ihslib/session.h"
#include "base.h"
#include "packet.h"
#include "session/channels/channel.h"

typedef struct IHS_SessionState {
    IHS_SessionConfig config;
    int mtu;
    uint8_t connectionId;
    uint8_t hostConnectionId;
} IHS_SessionState;

struct IHS_Session {
    IHS_Base base;
    IHS_SessionState state;
    uint8_t numChannels;
    IHS_SessionChannel *channels[16];
};

void IHS_SessionStop(IHS_Session *session);

void IHS_SessionPacketInitialize(IHS_Session *session, IHS_SessionPacket *packet);

uint32_t IHS_SessionPacketTimestamp(IHS_Session *session);

bool IHS_SessionSendPacket(IHS_Session *session, const IHS_SessionPacket *packet);

