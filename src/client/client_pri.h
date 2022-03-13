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

#include "ihslib/client.h"

#include "protobuf/discovery.pb-c.h"
#include "base.h"
#include "ihs_timer.h"


typedef void (IHS_MessageCallback)(IHS_Client *client, IHS_IPAddress ip, CMsgRemoteClientBroadcastHeader *header,
                                   ProtobufCMessage *message);

struct IHS_Client {
    IHS_Base base;
    IHS_ClientCallbacks callbacks;
    void *callbacksContext;

    struct {
        IHS_MessageCallback *discovery;
        IHS_MessageCallback *authorization;
        IHS_MessageCallback *streaming;
    } privCallbacks;
    struct {
        IHS_Timer *authorization;
        IHS_Timer *streaming;
    } taskHandles;
};

#define IHS_UNUSED(x) (void) (x)

#define IHS_ClientLog(client, ...) IHS_BaseLog((IHS_Base*) (client), __VA_ARGS__)

bool IHS_ClientSend(IHS_Client *client, IHS_SocketAddress address, ERemoteClientBroadcastMsg type,
                    ProtobufCMessage *message);

 bool IHS_ClientBroadcast(IHS_Client *client, ERemoteClientBroadcastMsg type,
                                       ProtobufCMessage *message);

bool IHS_ClientAuthorizationPubKey(IHS_Client *client, IHS_SteamUniverse universe, uint8_t *key, size_t *keyLen);

void IHS_ClientDiscoveryCallback(IHS_Client *client, IHS_IPAddress ip, CMsgRemoteClientBroadcastHeader *header,
                                 ProtobufCMessage *message);

void IHS_ClientAuthorizationCallback(IHS_Client *client, IHS_IPAddress ip, CMsgRemoteClientBroadcastHeader *header,
                                     ProtobufCMessage *message);

void IHS_ClientStreamingCallback(IHS_Client *client, IHS_IPAddress ip, CMsgRemoteClientBroadcastHeader *header,
                                 ProtobufCMessage *message);
