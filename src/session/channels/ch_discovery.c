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
#include "ch_discovery.h"
#include "session/session_pri.h"

static void OnDiscoveryReceived(IHS_SessionChannel *channel, const IHS_SessionPacket *packet);

static void OnConnectACK(IHS_SessionChannel *channel, const IHS_SessionPacket *packet);

static void OnUnconnected(IHS_SessionChannel *channel, const IHS_SessionPacket *packet);

static void OnDisconnect(IHS_SessionChannel *channel, const IHS_SessionPacket *packet);

static const IHS_SessionChannelClass Functions = {
        .onReceived = OnDiscoveryReceived,
        .instanceSize = sizeof(IHS_SessionChannel)
};

IHS_SessionChannel *IHS_SessionChannelDiscoveryCreate(IHS_Session *session) {
    return IHS_SessionChannelCreate(&Functions, session, IHS_SessionChannelIdDiscovery);
}

static void OnDiscoveryReceived(IHS_SessionChannel *channel, const IHS_SessionPacket *packet) {
    switch (packet->header.type) {
        case IHS_SessionPacketTypeConnectACK:
            OnConnectACK(channel, packet);
            break;
        case IHS_SessionPacketTypeUnconnected:
            OnUnconnected(channel, packet);
            break;
        case IHS_SessionPacketTypeDisconnect:
            OnDisconnect(channel, packet);
            break;
        default:
            break;
    }
}

static void OnConnectACK(IHS_SessionChannel *channel, const IHS_SessionPacket *packet) {
    IHS_Session *session = channel->session;
    if (session->state.connectionId != packet->header.dstConnectionId) return;
    session->state.hostConnectionId = packet->header.srcConnectionId;
    // TODO handshake
    printf("It's time to handshake\n");
}

static void OnUnconnected(IHS_SessionChannel *channel, const IHS_SessionPacket *packet) {

}

static void OnDisconnect(IHS_SessionChannel *channel, const IHS_SessionPacket *packet) {
    IHS_SessionStop(channel->session);
}
