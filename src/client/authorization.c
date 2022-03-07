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

#include "client_pri.h"
#include "pubkeys.h"
#include "crypto.h"

#include <string.h>
#include <malloc.h>

typedef struct IHS_AuthorizationState {
    IHS_HostInfo host;
    char deviceName[64];
    char pin[16];
} IHS_AuthorizationState;

static void AuthorizationRequestTimer(uv_timer_t *handle, int status);

static void AuthorizationRequestCleanup(uv_handle_t *handle);

static void AuthorizationConfigureTicket(IHS_Client *client, IHS_AuthorizationState *state,
                                         CMsgRemoteDeviceAuthorizationRequest__CKeyEscrowTicket *ticket);

bool IHS_ClientAuthorizationRequest(IHS_Client *client, const IHS_HostInfo *host, const char *pin) {
    if (client->taskHandles.authorization) {
        return false;
    }
    uv_timer_t *timer = malloc(sizeof(uv_timer_t));
    uv_timer_init(client->base.loop, timer);
    timer->close_cb = AuthorizationRequestCleanup;
    IHS_AuthorizationState *state = malloc(sizeof(IHS_AuthorizationState));
    state->host = *host;
    strncpy(state->deviceName, client->base.deviceName, sizeof(state->deviceName) - 1);
    strncpy(state->pin, pin, sizeof(state->pin) - 1);
    timer->data = state;
    IHS_BaseLock(&client->base);
    client->taskHandles.authorization = timer;
    IHS_BaseUnlock(&client->base);
    uv_timer_start(timer, AuthorizationRequestTimer, 0, 1000);
    return true;
}


void IHS_PRIV_ClientAuthorizationCallback(IHS_Client *client, IHS_HostIP ip,
                                          CMsgRemoteClientBroadcastHeader *header, ProtobufCMessage *message) {
    uv_timer_t *timer = client->taskHandles.authorization;
    if (!timer) return;
    if (header->msg_type != k_ERemoteDeviceAuthorizationResponse) {
        return;
    }
    CMsgRemoteDeviceAuthorizationResponse *resp = (CMsgRemoteDeviceAuthorizationResponse *) message;
    switch (resp->result) {
        case k_ERemoteDeviceAuthorizationInProgress:
            if (client->callbacks.authorizationInProgress) {
                client->callbacks.authorizationInProgress(client, client->callbacksContext);
            }
            return;
        case k_ERemoteDeviceAuthorizationSuccess:
            if (client->callbacks.authorizationSuccess) {
                client->callbacks.authorizationSuccess(client, resp->steamid, client->callbacksContext);
            }
            break;
        default:
            if (client->callbacks.authorizationFailed) {
                client->callbacks.authorizationFailed(client, (IHS_AuthorizationResult) resp->result,
                                                      client->callbacksContext);
            }
            break;
    }
    uv_timer_stop(timer);
}


bool IHS_PRIV_ClientAuthorizationPubKey(IHS_Client *client, int euniverse, uint8_t *key, size_t *keyLen) {
    IHS_UNUSED(client);
    switch (euniverse) {
        case 1:
            if (*keyLen < sizeof(IHS_AuthorizationPubKey1)) return false;
            memcpy(key, IHS_AuthorizationPubKey1, sizeof(IHS_AuthorizationPubKey1));
            *keyLen = sizeof(IHS_AuthorizationPubKey1);
            break;
        case 2:
            if (*keyLen < sizeof(IHS_AuthorizationPubKey2)) return false;
            memcpy(key, IHS_AuthorizationPubKey2, sizeof(IHS_AuthorizationPubKey2));
            *keyLen = sizeof(IHS_AuthorizationPubKey2);
            break;
        case 3:
        case 4:
            if (*keyLen < sizeof(IHS_AuthorizationPubKey3And4)) return false;
            memcpy(key, IHS_AuthorizationPubKey3And4, sizeof(IHS_AuthorizationPubKey3And4));
            *keyLen = sizeof(IHS_AuthorizationPubKey3And4);
            break;
        default:
            return false;
    }
    return true;
}

static void AuthorizationRequestTimer(uv_timer_t *handle, int status) {
    IHS_Client *client = handle->loop->data;
    IHS_AuthorizationState *state = handle->data;
    uint8_t pubKey[384];
    size_t pubKeyLen = sizeof(pubKey);
    IHS_PRIV_ClientAuthorizationPubKey(client, state->host.euniverse, pubKey, &pubKeyLen);
    ProtobufCBinaryData deviceToken = {.data=client->base.deviceToken, .len = sizeof(client->base.deviceToken)};

    /* Initialize and serialize ticket */
    CMsgRemoteDeviceAuthorizationRequest__CKeyEscrowTicket ticket =
            CMSG_REMOTE_DEVICE_AUTHORIZATION_REQUEST__CKEY_ESCROW__TICKET__INIT;
    AuthorizationConfigureTicket(client, state, &ticket);
    uint8_t serTicket[2048];
    size_t serTicketLen = protobuf_c_message_pack(&ticket.base, serTicket);

    /* RSA encrypt ticket data */
    uint8_t encryptedTicket[2048];
    ProtobufCBinaryData encryptedRequest = {.data=encryptedTicket, .len = sizeof(encryptedTicket)};
    IHS_CryptoRSAEncrypt(serTicket, serTicketLen, pubKey, pubKeyLen, encryptedRequest.data,
                         &encryptedRequest.len);

    CMsgRemoteDeviceAuthorizationRequest request = CMSG_REMOTE_DEVICE_AUTHORIZATION_REQUEST__INIT;
    request.device_name = state->deviceName;
    request.device_token = deviceToken;
    request.encrypted_request = encryptedRequest;

    IHS_HostAddress address = state->host.address;
    IHS_PRIV_ClientSend(client, address, k_ERemoteDeviceAuthorizationRequest, (ProtobufCMessage *) &request);
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

static void AuthorizationRequestCleanup(uv_handle_t *handle) {
    IHS_Client *client = handle->loop->data;
    IHS_BaseLock(&client->base);
    client->taskHandles.authorization = NULL;
    IHS_BaseUnlock(&client->base);
    free(handle->data);
    free(handle);
}