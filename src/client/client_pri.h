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

#include <uv.h>

#include "protobuf/discovery.pb-c.h"
#include "base.h"


typedef void (IHS_PRIV_MessageCallback)(IHS_Client *client, IHS_HostIP ip, CMsgRemoteClientBroadcastHeader *header,
                                        ProtobufCMessage *message);

struct IHS_Client {
    IHS_Base base;
    IHS_ClientCallbacks callbacks;
    void *callbacksContext;

    struct {
        IHS_PRIV_MessageCallback *discovery;
        IHS_PRIV_MessageCallback *authorization;
        IHS_PRIV_MessageCallback *streaming;
    } privCallbacks;
    struct {
        uv_timer_t *authorization;
        uv_timer_t *streaming;
    } taskHandles;
};

#define IHS_UNUSED(x) (void) (x)


bool IHS_PRIV_ClientSend(IHS_Client *client, IHS_HostAddress address, ERemoteClientBroadcastMsg type,
                         ProtobufCMessage *message);

static inline bool IHS_PRIV_ClientBroadcast(IHS_Client *client, ERemoteClientBroadcastMsg type,
                                            ProtobufCMessage *message) {
    IHS_HostAddress address = {{IHS_HostIPv4, {.v4 = INADDR_BROADCAST}}, 27036};
    return IHS_PRIV_ClientSend(client, address, type, message);
}

bool IHS_PRIV_ClientAuthorizationPubKey(IHS_Client *client, int euniverse, uint8_t *key, size_t *keyLen);

void IHS_PRIV_ClientDiscoveryCallback(IHS_Client *client, IHS_HostIP ip, CMsgRemoteClientBroadcastHeader *header,
                                      ProtobufCMessage *message);

void IHS_PRIV_ClientAuthorizationCallback(IHS_Client *client, IHS_HostIP ip, CMsgRemoteClientBroadcastHeader *header,
                                          ProtobufCMessage *message);

void IHS_PRIV_ClientStreamingCallback(IHS_Client *client, IHS_HostIP ip, CMsgRemoteClientBroadcastHeader *header,
                                      ProtobufCMessage *message);
