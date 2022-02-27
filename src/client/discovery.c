#include "client_pri.h"

#include <string.h>


void IHS_ClientDiscoveryBroadcast(IHS_Client *client) {
    CMsgRemoteClientBroadcastDiscovery discovery;
    cmsg_remote_client_broadcast_discovery__init(&discovery);
    discovery.has_seq_num = 1;
    discovery.seq_num = 0;

    IHS_ClientPriSend(client, "255.255.255.255", k_ERemoteClientBroadcastMsgDiscovery, (ProtobufCMessage *) &discovery);
}


void IHS_ClientPriDiscoveryCallback(IHS_Client *client, IHS_HostIP address, CMsgRemoteClientBroadcastHeader *header,
                                    ProtobufCMessage *message) {
    if (header->msg_type == k_ERemoteClientBroadcastMsgStatus) {
        CMsgRemoteClientBroadcastStatus *status = (CMsgRemoteClientBroadcastStatus *) message;
        char buf[64];
        inet_ntop(address.type, &address.value, buf, 64);

        IHS_HostInfo info;
        info.instanceId = header->instance_id;
        info.address.ip = address;
        info.address.port = status->connect_port;
        info.euniverse = status->euniverse;
        info.gamesRunning = status->games_running;
        strncpy(info.hostname, status->hostname, sizeof(info.hostname) - 1);
        info.hostname[sizeof(info.hostname) - 1] = '\0';
        client->callbacks.hostDiscovered(client, info);
    }
}