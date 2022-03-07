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

#include "client_pri.h"

#include <string.h>


bool IHS_ClientDiscoveryBroadcast(IHS_Client *client) {
    CMsgRemoteClientBroadcastDiscovery discovery = CMSG_REMOTE_CLIENT_BROADCAST_DISCOVERY__INIT;
    discovery.has_seq_num = 1;
    discovery.seq_num = 0;

    return IHS_PRIV_ClientBroadcast(client, k_ERemoteClientBroadcastMsgDiscovery, (ProtobufCMessage *) &discovery);
}


void IHS_PRIV_ClientDiscoveryCallback(IHS_Client *client, IHS_HostIP ip, CMsgRemoteClientBroadcastHeader *header,
                                      ProtobufCMessage *message) {
    if (header->msg_type == k_ERemoteClientBroadcastMsgStatus) {
        CMsgRemoteClientBroadcastStatus *status = (CMsgRemoteClientBroadcastStatus *) message;
        char buf[64];
        inet_ntop(ip.type, &ip.value, buf, 64);

        IHS_HostInfo info;
        info.clientId = header->client_id;
        info.instanceId = header->instance_id;
        info.address.ip = ip;
        info.address.port = status->connect_port;
        info.euniverse = status->euniverse;
        info.gamesRunning = status->games_running;
        strncpy(info.hostname, status->hostname, sizeof(info.hostname) - 1);
        info.hostname[sizeof(info.hostname) - 1] = '\0';
        client->callbacks.hostDiscovered(client, info, client->callbacksContext);
    }
}