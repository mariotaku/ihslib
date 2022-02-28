#pragma once

#include <stdbool.h>

#include <sys/socket.h>
#include <netinet/in.h>

typedef struct IHS_ClientConfig {
    uint64_t deviceId;
    const uint8_t *secretKey;
    const char *deviceName;
} IHS_ClientConfig;

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
    uint64_t clientId;
    uint64_t instanceId;
    IHS_HostAddress address;
    char hostname[64];
    uint8_t euniverse;
    bool gamesRunning;
} IHS_HostInfo;