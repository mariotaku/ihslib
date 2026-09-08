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

#include "client_pri.h"
#include "protobuf/pb_utils.h"

#include <string.h>

static uint64_t DiscoveryTimerRun(int runCount, IHS_Client *client);

static void DiscoveryTimerEnd(IHS_Client *client);

static bool DiscoveryBroadcast(IHS_Client *client);

static bool DiscoverySendTo(IHS_Client *client, const IHS_SocketAddress *address);

/**
 * Steam's discovery port. Both the broadcast and the unicast probe go here; see
 * IHS_ClientBroadcast's address literal in client.c.
 */
static const uint16_t DiscoveryPort = 27036;

bool IHS_ClientStartDiscovery(IHS_Client *client, uint32_t interval) {
    IHS_BaseLock(&client->base);
    if (client->discoveryTimer != NULL) {
        IHS_BaseUnlock(&client->base);
        return false;
    }
    client->discoveryInterval = interval;
    client->discoveryTimer = IHS_TimerTaskStart(client->timers, (IHS_TimerRunFunction *) DiscoveryTimerRun,
                                                (IHS_TimerEndFunction *) DiscoveryTimerEnd, 0, client);
    IHS_BaseUnlock(&client->base);
    return true;
}

bool IHS_ClientStopDiscovery(IHS_Client *client) {
    IHS_BaseLock(&client->base);
    if (client->discoveryTimer == NULL) {
        IHS_BaseUnlock(&client->base);
        return false;
    }
    client->discoveryInterval = 0;
    client->discoveryTimer = NULL;
    IHS_BaseUnlock(&client->base);
    return true;
}

bool IHS_ClientDiscoverAt(IHS_Client *client, const IHS_IPAddress *ip) {
    if (ip == NULL) {
        return false;
    }
    IHS_SocketAddress address = {.ip = *ip, .port = DiscoveryPort};
    IHS_ClientLog(client, IHS_LogLevelVerbose, "Discovery", "Send unicast probe");
    return DiscoverySendTo(client, &address);
}


void IHS_ClientDiscoveryCallback(IHS_Client *client, const IHS_SocketAddress *address,
                                 CMsgRemoteClientBroadcastHeader *header, ProtobufCMessage *message) {
    if (header->msg_type == k_ERemoteClientBroadcastMsgStatus) {
        CMsgRemoteClientBroadcastStatus *status = (CMsgRemoteClientBroadcastStatus *) message;
        IHS_HostInfo info;
        info.clientId = header->client_id;
        info.instanceId = header->instance_id;
        info.address = *address;
        info.ostype = status->ostype;
        info.universe = status->euniverse;
        info.gamesRunning = status->games_running;
        strncpy(info.hostname, status->hostname, sizeof(info.hostname) - 1);
        info.hostname[sizeof(info.hostname) - 1] = '\0';
        if (client->callbacks.discovery && client->callbacks.discovery->discovered) {
            client->callbacks.discovery->discovered(client, &info, client->callbackContexts.discovery);
        }
    }
}

static uint64_t DiscoveryTimerRun(int runCount, IHS_Client *client) {
    if (client->discoveryTimer == NULL) {
        return 0;
    }
    IHS_ClientLog(client, IHS_LogLevelVerbose, "Discovery", "Send broadcast");
    DiscoveryBroadcast(client);
    return client->discoveryInterval;
}

static void DiscoveryTimerEnd(IHS_Client *client) {
    IHS_BaseLock(&client->base);
    client->discoveryTimer = NULL;
    IHS_BaseUnlock(&client->base);
}

static bool DiscoveryBroadcast(IHS_Client *client) {
    return DiscoverySendTo(client, NULL);
}

/**
 * Send one discovery probe. A NULL address broadcasts, which is what the periodic timer does;
 * IHS_ClientDiscoverAt passes a host address instead, for networks where the broadcast never
 * arrives (AP isolation, or the host on a different VLAN).
 *
 * The sequence number is bumped under the base lock because the caller may be the application
 * thread while the discovery timer is running on the timer thread.
 */
static bool DiscoverySendTo(IHS_Client *client, const IHS_SocketAddress *address) {
    IHS_BaseLock(&client->base);
    uint32_t seqNum = client->discoverySeq++;
    IHS_BaseUnlock(&client->base);

    CMsgRemoteClientBroadcastDiscovery discovery = CMSG_REMOTE_CLIENT_BROADCAST_DISCOVERY__INIT;
    PROTOBUF_C_SET_VALUE(discovery, seq_num, seqNum);

    if (address == NULL) {
        return IHS_ClientBroadcast(client, k_ERemoteClientBroadcastMsgDiscovery, (ProtobufCMessage *) &discovery);
    }
    return IHS_ClientSend(client, *address, k_ERemoteClientBroadcastMsgDiscovery, (ProtobufCMessage *) &discovery);
}