#include "client_pri.h"

#include <string.h>


void IHS_ClientDiscoveryBroadcast(IHS_Client *client) {
    CMsgRemoteClientBroadcastDiscovery discovery = CMSG_REMOTE_CLIENT_BROADCAST_DISCOVERY__INIT;
    discovery.has_seq_num = 1;
    discovery.seq_num = 0;

    IHS_PRIV_ClientBroadcast(client, k_ERemoteClientBroadcastMsgDiscovery, (ProtobufCMessage *) &discovery);
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
        client->callbacks.hostDiscovered(client, info);
    }
}