#pragma once

#include <stdint.h>
#include <netinet/in.h>
#include <stdbool.h>

#include "common.h"

typedef struct IHS_Client IHS_Client;

typedef struct IHS_StreamingRequest {
    char pin[16];
    struct {
        bool video;
        bool audio;
        bool input;
    } streamingEnable;
    struct {
        int32_t x;
        int32_t y;
    } maxResolution;
    int32_t audioChannelCount;
    int32_t gamepadCount;
} IHS_StreamingRequest;

/**
 * Corresponding to ERemoteDeviceAuthorizationResult
 */
typedef enum IHS_AuthorizationResult {
    IHS_AuthorizationSuccess = 0,
    IHS_AuthorizationDenied = 1,
    IHS_AuthorizationNotLoggedIn = 2,
    IHS_AuthorizationOffline = 3,
    IHS_AuthorizationBusy = 4,
    IHS_AuthorizationInProgress = 5,
    IHS_AuthorizationTimedOut = 6,
    IHS_AuthorizationFailed = 7,
    IHS_AuthorizationCanceled = 8,
} IHS_AuthorizationResult;

typedef enum IHS_StreamingResult {
    IHS_StreamingSuccess = 0,
    IHS_StreamingUnauthorized = 1,
    IHS_StreamingScreenLocked = 2,
    IHS_StreamingFailed = 3,
    IHS_StreamingBusy = 4,
    IHS_StreamingInProgress = 5,
    IHS_StreamingCanceled = 6,
    IHS_StreamingDriversNotInstalled = 7,
    IHS_StreamingDisabled = 8,
    IHS_StreamingBroadcastingActive = 9,
    IHS_StreamingVRActive = 10,
    IHS_StreamingPINRequired = 11,
    IHS_StreamingTransportUnavailable = 12,
    IHS_StreamingInvisible = 13,
    IHS_StreamingGameLaunchFailed = 14,
} IHS_StreamingResult;

typedef struct IHS_ClientCallbacks {
    void (*hostDiscovered)(IHS_Client *client, IHS_HostInfo host);

    void (*authorizationInProgress)(IHS_Client *client);

    void (*authorizationSuccess)(IHS_Client *client, uint64_t steamId);

    void (*authorizationFailed)(IHS_Client *client, IHS_AuthorizationResult result);

    void (*streamingInProgress)(IHS_Client *client);

    void (*streamingSuccess)(IHS_Client *client, IHS_HostInfo host, uint16_t sessionPort,
                             const uint8_t *sessionKey, size_t sessionKeyLen);

    void (*streamingFailed)(IHS_Client *client, IHS_StreamingResult result);
} IHS_ClientCallbacks;


IHS_Client *IHS_ClientCreate(const IHS_ClientConfig *config);

void IHS_ClientStop(IHS_Client *client);

void IHS_ClientDestroy(IHS_Client *client);

void IHS_ClientSetCallbacks(IHS_Client *client, const IHS_ClientCallbacks *callbacks);

/* ----------------------------------------------------
 * - Discovery functions
 * ---------------------------------------------------- */

void IHS_ClientDiscoveryBroadcast(IHS_Client *client);

/* ----------------------------------------------------
 * - Authorization functions
 * ---------------------------------------------------- */

/**
 * Request authorization
 * @param client Client instance
 * @param host Host information
 * @param pin PIN code
 */
bool IHS_ClientAuthorizationRequest(IHS_Client *client, const IHS_HostInfo *host, const char *pin);


/* ----------------------------------------------------
 * - Authorization functions
 * ---------------------------------------------------- */

bool IHS_ClientStreamingRequest(IHS_Client *client, const IHS_HostInfo *host, const IHS_StreamingRequest *request);