#pragma once

#include "ihslib/client.h"

#include <sys/socket.h>

#include <uv.h>

#include "protobuf/discovery.pb-c.h"


typedef void (IHS_PRIV_MessageCallback)(IHS_Client *client, IHS_HostIP ip, CMsgRemoteClientBroadcastHeader *header,
                                        ProtobufCMessage *message);

struct IHS_Client {
    uint64_t deviceId;
    uint8_t secretKey[32];
    char deviceName[64];
    uint8_t deviceToken[32];
    IHS_ClientCallbacks callbacks;

    struct {
        IHS_PRIV_MessageCallback *discovery;
        IHS_PRIV_MessageCallback *authorization;
        IHS_PRIV_MessageCallback *streaming;
    } privCallbacks;
    uv_loop_t *loop;
    uv_thread_t workerThread;
    uv_udp_t udp;
    uv_mutex_t mutex;
    struct {
        uv_timer_t *authorization;
        uv_timer_t *streaming;
    } taskHandles;
};

#define IHS_UNUSED(x) (void) (x)


void IHS_PRIV_ClientSend(IHS_Client *client, IHS_HostAddress address, ERemoteClientBroadcastMsg type,
                         ProtobufCMessage *message);

static inline void IHS_PRIV_ClientBroadcast(IHS_Client *client, ERemoteClientBroadcastMsg type,
                                            ProtobufCMessage *message) {
    IHS_HostAddress address = {{IHS_HostIPv4, {.v4 = INADDR_BROADCAST}}, 27036};
    IHS_PRIV_ClientSend(client, address, type, message);
}

bool IHS_PRIV_ClientAuthorizationPubKey(IHS_Client *client, int euniverse, uint8_t *key, size_t *keyLen);

void IHS_PRIV_ClientDiscoveryCallback(IHS_Client *client, IHS_HostIP ip, CMsgRemoteClientBroadcastHeader *header,
                                      ProtobufCMessage *message);

void IHS_PRIV_ClientAuthorizationCallback(IHS_Client *client, IHS_HostIP ip, CMsgRemoteClientBroadcastHeader *header,
                                          ProtobufCMessage *message);

void IHS_PRIV_ClientStreamingCallback(IHS_Client *client, IHS_HostIP ip, CMsgRemoteClientBroadcastHeader *header,
                                      ProtobufCMessage *message);

void IHS_PRIV_ClientLock(IHS_Client *client);

void IHS_PRIV_ClientUnlock(IHS_Client *client);