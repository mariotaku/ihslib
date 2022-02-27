#pragma once

#include "ihslib/client.h"

#include <sys/socket.h>

#include <uv.h>

#include "protobuf/discovery.pb-c.h"


typedef void (IHS_ClientCallback)(IHS_Client *client, IHS_HostIP address, CMsgRemoteClientBroadcastHeader *header,
                                  ProtobufCMessage *message);

struct IHS_Client {
    uint64_t deviceId;
    uint8_t secretKey[32];
    char deviceName[64];
    IHS_ClientCallbacks callbacks;

    struct {
        IHS_ClientCallback *discovery;
        IHS_ClientCallback *authorization;
        IHS_ClientCallback *session;
    } privCallbacks;
    uv_loop_t *loop;
    uv_thread_t workerThread;
    uv_udp_t udp;
    uv_mutex_t mutex;
    struct {
        uv_timer_t *authorization;
    } timers;
};

#define IHS_UNUSED(x) (void) (x)

void IHS_ClientPriSend(IHS_Client *client, const char *ip, ERemoteClientBroadcastMsg type, ProtobufCMessage *message);

bool IHS_ClientPriAuthorizationPubKey(IHS_Client *client, int euniverse, uint8_t *key, size_t *keyLen);

void IHS_ClientPriDeviceToken(IHS_Client *client, uint8_t *token, size_t *tokenLen);

void IHS_ClientPriDiscoveryCallback(IHS_Client *client, IHS_HostIP address, CMsgRemoteClientBroadcastHeader *header,
                                    ProtobufCMessage *message);

void IHS_ClientPriAuthorizationCallback(IHS_Client *client, IHS_HostIP address, CMsgRemoteClientBroadcastHeader *header,
                                        ProtobufCMessage *message);

void IHS_ClientLock(IHS_Client *client);

void IHS_ClientUnlock(IHS_Client *client);