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
#include "pubkeys.h"
#include "crypto.h"
#include "authorization_kx.h"

#include <string.h>
#include <stdlib.h>

typedef struct IHS_AuthorizationState {
    IHS_Client *client;
    IHS_HostInfo host;
    char deviceName[64];
    char pin[16];
    uint32_t requestId;
    IHS_AuthorizationKeyExchange keyExchange;
} IHS_AuthorizationState;

static uint64_t AuthorizationRequestTimer(int runCount, void *data);

static void AuthorizationRequestCleanup(void *data);

static void AuthorizationConfigureTicket(IHS_Client *client, IHS_AuthorizationState *state,
                                         CMsgRemoteDeviceAuthorizationRequest__CKeyEscrowTicket *ticket);

static void AuthorizationKeyExchange(IHS_Client *client, IHS_AuthorizationState *state,
                                     const CMsgRemoteDeviceAuthorizationResponse *resp);

bool IHS_ClientAuthorizationRequest(IHS_Client *client, const IHS_HostInfo *host, const char *pin) {
    if (client->taskHandles.authorization) {
        return false;
    }
    IHS_AuthorizationState *state = calloc(1, sizeof(IHS_AuthorizationState));
    state->client = client;
    state->host = *host;
    strncpy(state->deviceName, client->base.deviceName, sizeof(state->deviceName) - 1);
    strncpy(state->pin, pin, sizeof(state->pin) - 1);
    state->requestId = IHS_CryptoRandomUInt32();
    if (!IHS_AuthorizationKeyExchangeInit(&state->keyExchange, state->pin)) {
        IHS_AuthorizationKeyExchangeClear(&state->keyExchange);
        free(state);
        return false;
    }
    IHS_BaseLock(&client->base);
    client->taskHandles.authorization = IHS_TimerTaskStart(client->timers, AuthorizationRequestTimer,
                                                           AuthorizationRequestCleanup, 0, state);
    IHS_BaseUnlock(&client->base);
    return true;
}

bool IHS_ClientAuthorizationCancel(IHS_Client *client) {
    if (!client->taskHandles.authorization) {
        return false;
    }
    IHS_TimerTask *task = client->taskHandles.authorization;
    IHS_AuthorizationState *state = IHS_TimerTaskGetContext(task);

    CMsgRemoteDeviceAuthorizationCancelRequest request = CMSG_REMOTE_DEVICE_AUTHORIZATION_CANCEL_REQUEST__INIT;
    IHS_SocketAddress address = state->host.address;

    IHS_TimerTaskStop(task);

    IHS_ClientSend(client, address, k_ERemoteDeviceAuthorizationCancelRequest, (ProtobufCMessage *) &request);
    return true;
}

void IHS_ClientAuthorizationCallback(IHS_Client *client, const IHS_SocketAddress *address,
                                     CMsgRemoteClientBroadcastHeader *header, ProtobufCMessage *message) {
    IHS_UNUSED(address);
    IHS_TimerTask *task = client->taskHandles.authorization;
    if (!task) return;
    if (header->msg_type != k_ERemoteDeviceAuthorizationResponse) {
        return;
    }
    IHS_AuthorizationState *state = IHS_TimerTaskGetContext(task);
    CMsgRemoteDeviceAuthorizationResponse *resp = (CMsgRemoteDeviceAuthorizationResponse *) message;
    if (resp == NULL) return;
    if (resp->has_auth_key) {
        // Like the official client, a response carrying the host's half of the key exchange is handled as such,
        // whatever its result says.
        AuthorizationKeyExchange(client, state, resp);
        IHS_TimerTaskStop(task);
        return;
    }
    switch (resp->result) {
        case k_ERemoteDeviceAuthorizationInProgress:
            if (client->callbacks.authorization && client->callbacks.authorization->progress) {
                client->callbacks.authorization->progress(client, &state->host, client->callbackContexts.authorization);
            }
            return;
        case k_ERemoteDeviceAuthorizationSuccess:
            if (client->callbacks.authorization && client->callbacks.authorization->success) {
                client->callbacks.authorization->success(client, &state->host, resp->steamid,
                                                         client->callbackContexts.authorization);
            }
            break;
        default:
            if (client->callbacks.authorization && client->callbacks.authorization->failed) {
                client->callbacks.authorization->failed(client, &state->host, (IHS_AuthorizationResult) resp->result,
                                                        client->callbackContexts.authorization);
            }
            break;
    }
    IHS_TimerTaskStop(task);
}


bool IHS_ClientAuthorizationPubKey(IHS_Client *client, IHS_SteamUniverse universe, uint8_t *key, size_t *keyLen) {
    IHS_UNUSED(client);
    switch (universe) {
        case IHS_SteamUniversePublic:
            if (*keyLen < sizeof(IHS_AuthorizationPubKeyPublic)) return false;
            memcpy(key, IHS_AuthorizationPubKeyPublic, sizeof(IHS_AuthorizationPubKeyPublic));
            *keyLen = sizeof(IHS_AuthorizationPubKeyPublic);
            break;
        case IHS_SteamUniverseBeta:
            if (*keyLen < sizeof(IHS_AuthorizationPubKeyBeta)) return false;
            memcpy(key, IHS_AuthorizationPubKeyBeta, sizeof(IHS_AuthorizationPubKeyBeta));
            *keyLen = sizeof(IHS_AuthorizationPubKeyBeta);
            break;
        case IHS_SteamUniverseInternal:
        case IHS_SteamUniverseDev:
            if (*keyLen < sizeof(IHS_AuthorizationPubKeyDev)) return false;
            memcpy(key, IHS_AuthorizationPubKeyDev, sizeof(IHS_AuthorizationPubKeyDev));
            *keyLen = sizeof(IHS_AuthorizationPubKeyDev);
            break;
        default:
            return false;
    }
    return true;
}

static uint64_t AuthorizationRequestTimer(int runCount, void *data) {
    (void) runCount;
    IHS_AuthorizationState *state = data;
    IHS_Client *client = state->client;
    uint8_t pubKey[384];
    size_t pubKeyLen = sizeof(pubKey);
    IHS_ClientAuthorizationPubKey(client, state->host.universe, pubKey, &pubKeyLen);
    ProtobufCBinaryData deviceToken = {.data = client->base.deviceToken, .len = sizeof(client->base.deviceToken)};

    /* Initialize and serialize ticket */
    CMsgRemoteDeviceAuthorizationRequest__CKeyEscrowTicket ticket =
            CMSG_REMOTE_DEVICE_AUTHORIZATION_REQUEST__CKEY_ESCROW__TICKET__INIT;
    AuthorizationConfigureTicket(client, state, &ticket);
    uint8_t serTicket[2048];
    size_t serTicketLen = protobuf_c_message_pack(&ticket.base, serTicket);

    /* RSA encrypt ticket data */
    uint8_t encryptedTicket[2048];
    ProtobufCBinaryData encryptedRequest = {.data = encryptedTicket, .len = sizeof(encryptedTicket)};
    IHS_CryptoRSAEncrypt(serTicket, serTicketLen, pubKey, pubKeyLen, encryptedRequest.data,
                         &encryptedRequest.len);

    uint8_t authKey[IHS_CRYPTO_X25519_KEY_SIZE];
    IHS_AuthorizationKeyExchangeMaskedKey(&state->keyExchange, authKey);

    CMsgRemoteDeviceAuthorizationRequest request = CMSG_REMOTE_DEVICE_AUTHORIZATION_REQUEST__INIT;
    request.device_name = state->deviceName;
    request.device_token = deviceToken;
    request.encrypted_request = encryptedRequest;
    request.has_auth_key = true;
    request.auth_key.data = authKey;
    request.auth_key.len = sizeof(authKey);
    request.has_request_id = true;
    request.request_id = state->requestId;

    IHS_SocketAddress address = state->host.address;
    IHS_ClientSend(client, address, k_ERemoteDeviceAuthorizationRequest, (ProtobufCMessage *) &request);
    return 1000;
}

static void AuthorizationConfigureTicket(IHS_Client *client, IHS_AuthorizationState *state,
                                         CMsgRemoteDeviceAuthorizationRequest__CKeyEscrowTicket *ticket) {
    ticket->has_password = true;
    ticket->password.len = strlen(state->pin);
    ticket->password.data = (uint8_t *) state->pin;

    ticket->has_identifier = true;
    ticket->identifier = client->base.deviceId;

    ticket->has_payload = true;
    ticket->payload.len = sizeof(client->base.secretKey);
    ticket->payload.data = client->base.secretKey;

    ticket->has_usage = true;
    ticket->usage = k_EKeyEscrowUsageStreamingDevice;

    ticket->device_name = client->base.deviceName;
}

static void AuthorizationKeyExchange(IHS_Client *client, IHS_AuthorizationState *state,
                                     const CMsgRemoteDeviceAuthorizationResponse *resp) {
    uint8_t secretKey[IHS_CRYPTO_SHA256_SIZE];
    bool ok = resp->has_device_token &&
              IHS_AuthorizationKeyExchangeComplete(&state->keyExchange, resp->auth_key.data, resp->auth_key.len,
                                                   resp->device_token.data, resp->device_token.len,
                                                   client->base.deviceId, secretKey);

    CMsgRemoteDeviceAuthorizationConfirmed confirmed = CMSG_REMOTE_DEVICE_AUTHORIZATION_CONFIRMED__INIT;
    confirmed.result = ok ? k_ERemoteDeviceAuthorizationSuccess : k_ERemoteDeviceAuthorizationDenied;
    IHS_ClientSend(client, state->host.address, k_ERemoteDeviceAuthorizationConfirmed,
                   (ProtobufCMessage *) &confirmed);

    if (!ok) {
        IHS_ClientLog(client, IHS_LogLevelWarn, "Client", "Key exchange with host %s failed (wrong PIN?)",
                      state->host.hostname);
        if (client->callbacks.authorization && client->callbacks.authorization->failed) {
            client->callbacks.authorization->failed(client, &state->host, IHS_AuthorizationDenied,
                                                    client->callbackContexts.authorization);
        }
        return;
    }

    IHS_BaseSetSecretKey(&client->base, secretKey);
    memset(secretKey, 0, sizeof(secretKey));
    if (client->callbacks.authorization && client->callbacks.authorization->success) {
        client->callbacks.authorization->success(client, &state->host, resp->steamid,
                                                 client->callbackContexts.authorization);
    }
}

static void AuthorizationRequestCleanup(void *data) {
    IHS_AuthorizationState *state = data;
    IHS_Client *client = state->client;
    IHS_BaseLock(&client->base);
    client->taskHandles.authorization = NULL;
    IHS_BaseUnlock(&client->base);
    IHS_AuthorizationKeyExchangeClear(&state->keyExchange);
    free(data);
}