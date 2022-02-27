#pragma once

#include <stdint.h>
#include <netinet/in.h>
#include <stdbool.h>

typedef struct IHS_Client IHS_Client;

typedef struct IHS_HostIP {
    enum {
        IHS_HostIPv4 = AF_INET,
        IHS_HostIPv6 = AF_INET6,
    } type;
    union {
        struct in_addr v4;
        struct in6_addr v6;
    } value;
} IHS_HostIP;

typedef struct IHS_HostAddress {
    IHS_HostIP ip;
    uint16_t port;
} IHS_HostAddress;

typedef struct IHS_HostInfo {
    uint64_t instanceId;
    IHS_HostAddress address;
    char hostname[64];
    uint8_t euniverse;
    bool gamesRunning;
} IHS_HostInfo;

/**
 * Corresponding to ERemoteDeviceAuthorizationResult
 */
typedef enum IHS_AuthorizationResult {
    IHS_AuthorizationDenied = 1,
    IHS_AuthorizationNotLoggedIn = 2,
    IHS_AuthorizationOffline = 3,
    IHS_AuthorizationBusy = 4,
    IHS_AuthorizationTimedOut = 6,
    IHS_AuthorizationFailed = 7,
    IHS_AuthorizationCanceled = 8,
} IHS_AuthorizationResult;

typedef struct IHS_ClientCallbacks {
    void (*hostDiscovered)(IHS_Client *client, IHS_HostInfo host);

    void (*authorizationInProgress)(IHS_Client *client);

    void (*authorizationSuccess)(IHS_Client *client, uint64_t steamId);

    void (*authorizationFailed)(IHS_Client *client, IHS_AuthorizationResult result);
} IHS_ClientCallbacks;


IHS_Client *IHS_ClientCreate(uint64_t deviceId, const uint8_t *secretKey, const char*deviceName);

void IHS_ClientDestroy(IHS_Client *client);

void IHS_ClientSetCallbacks(IHS_Client *client, const IHS_ClientCallbacks *callbacks);

void IHS_ClientDiscoveryBroadcast(IHS_Client *client);

void IHS_ClientStartAuthorization(IHS_Client *client, const IHS_HostInfo *host, const char *pin);