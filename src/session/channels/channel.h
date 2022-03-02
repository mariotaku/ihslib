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
#include "session/packet.h"

typedef struct IHS_SessionChannel IHS_SessionChannel;

typedef struct IHS_SessionChannelClass {
    void (*Send)(struct IHS_SessionChannel *channel);

    void (*onReceived)(struct IHS_SessionChannel *channel, const IHS_SessionPacket *packet);

    size_t instanceSize;
} IHS_SessionChannelClass;

struct IHS_SessionChannel {
    IHS_SessionChannelClass cls;
    IHS_SessionChannelId id;
    IHS_Session *session;
};

IHS_SessionChannel *IHS_SessionChannelCreate(const IHS_SessionChannelClass *cls, IHS_Session *session,
                                             IHS_SessionChannelId id);

void IHS_SessionChannelDestroy(IHS_SessionChannel *channel);

IHS_SessionChannel *IHS_SessionChannelFor(IHS_Session *session, IHS_SessionChannelId channelId);

void IHS_SessionChannelReceivedPacket(IHS_SessionChannel *channel, const IHS_SessionPacket *packet);

void IHS_SessionChannelReceivedPacketBase(IHS_SessionChannel *channel, const IHS_SessionPacket *packet);

void IHS_SessionChannelSendBytes(IHS_SessionChannel *channel, IHS_SessionPacketType type, bool hasCrc,
                                 const uint8_t *body, size_t bodyLen);