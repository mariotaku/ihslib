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

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

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
    void (*hostDiscovered)(IHS_Client *client, IHS_HostInfo host, void *context);

    void (*authorizationInProgress)(IHS_Client *client, void *context);

    void (*authorizationSuccess)(IHS_Client *client, uint64_t steamId, void *context);

    void (*authorizationFailed)(IHS_Client *client, IHS_AuthorizationResult result, void *context);

    void (*streamingInProgress)(IHS_Client *client, void *context);

    void (*streamingSuccess)(IHS_Client *client, IHS_SocketAddress address,
                             const uint8_t *sessionKey, size_t sessionKeyLen, void *context);

    void (*streamingFailed)(IHS_Client *client, IHS_StreamingResult result, void *context);
} IHS_ClientCallbacks;

typedef struct IHS_ClientDiscoveryCallbacks {
    void (*discovered)(IHS_Client *client, IHS_HostInfo host, void *context);
} IHS_ClientDiscoveryCallbacks;

typedef struct IHS_ClientAuthorizationCallbacks {
    void (*progress)(IHS_Client *client, void *context);

    void (*success)(IHS_Client *client, uint64_t steamId, void *context);

    void (*failed)(IHS_Client *client, IHS_AuthorizationResult result, void *context);
} IHS_ClientAuthorizationCallbacks;

typedef struct IHS_ClientStreamingCallbacks {
    void (*progress)(IHS_Client *client, void *context);

    void (*success)(IHS_Client *client, IHS_SocketAddress address,
                             const uint8_t *sessionKey, size_t sessionKeyLen, void *context);

    void (*failed)(IHS_Client *client, IHS_StreamingResult result, void *context);
} IHS_ClientStreamingCallbacks;


IHS_Client *IHS_ClientCreate(const IHS_ClientConfig *config);

void IHS_ClientSetLogFunction(IHS_Client *client, IHS_LogFunction *logFunction);

void IHS_ClientRun(IHS_Client *client);

void IHS_ClientStop(IHS_Client *client);

void IHS_ClientThreadedStart(IHS_Client *client);

void IHS_ClientThreadedJoin(IHS_Client *client);

void IHS_ClientDestroy(IHS_Client *client);

void IHS_ClientSetDiscoveryCallbacks(IHS_Client *client, const IHS_ClientDiscoveryCallbacks *callbacks, void *context);

void IHS_ClientSetAuthorizationCallbacks(IHS_Client *client, const IHS_ClientAuthorizationCallbacks *callbacks, void *context);

void IHS_ClientSetStreamingCallbacks(IHS_Client *client, const IHS_ClientStreamingCallbacks *callbacks, void *context);

const char *IHS_ClientError(IHS_Client *client);

/* ----------------------------------------------------
 * - Discovery functions
 * ---------------------------------------------------- */

bool IHS_ClientDiscoveryBroadcast(IHS_Client *client);

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
