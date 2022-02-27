#include "client_pri.h"
#include "pubkeys.h"
#include "ihslib/crypto.h"

#include <string.h>
#include <malloc.h>

typedef struct IHS_AuthorizationState {
    IHS_HostInfo host;
    char deviceName[64];
    char pin[16];
} IHS_AuthorizationState;

static void IHS_AuthorizationTimer(uv_timer_t *handle, int status);

static void IHS_AuthorizationCleanup(uv_handle_t *handle);

static void IHS_AuthorizationConfigureTicket(IHS_Client *client, IHS_AuthorizationState *state,
                                             CMsgRemoteDeviceAuthorizationRequest__CKeyEscrowTicket *ticket);

void IHS_ClientStartAuthorization(IHS_Client *client, const IHS_HostInfo *host, const char *pin) {
    uv_timer_t *timer = malloc(sizeof(uv_timer_t));
    uv_timer_init(client->loop, timer);
    timer->close_cb = IHS_AuthorizationCleanup;
    IHS_AuthorizationState *state = malloc(sizeof(IHS_AuthorizationState));
    state->host = *host;
    strncpy(state->deviceName, client->deviceName, sizeof(state->deviceName) - 1);
    strncpy(state->pin, pin, sizeof(state->pin) - 1);
    timer->data = state;
    IHS_ClientLock(client);
    client->timers.authorization = timer;
    IHS_ClientUnlock(client);
    uv_timer_start(timer, IHS_AuthorizationTimer, 0, 1000);
}


void IHS_ClientPriAuthorizationCallback(IHS_Client *client, IHS_HostIP address, CMsgRemoteClientBroadcastHeader *header,
                                        ProtobufCMessage *message) {
    uv_timer_t *timer = client->timers.authorization;
    if (!timer) return;
    if (header->msg_type != k_ERemoteDeviceAuthorizationResponse) {
        return;
    }
    CMsgRemoteDeviceAuthorizationResponse *resp = (CMsgRemoteDeviceAuthorizationResponse *) message;
    switch (resp->result) {
        case k_ERemoteDeviceAuthorizationInProgress:
            if (client->callbacks.authorizationInProgress) {
                client->callbacks.authorizationInProgress(client);
            }
            return;
        case k_ERemoteDeviceAuthorizationSuccess:
            if (client->callbacks.authorizationSuccess) {
                client->callbacks.authorizationSuccess(client, resp->steamid);
            }
            break;
        default:
            if (client->callbacks.authorizationFailed) {
                client->callbacks.authorizationFailed(client, (IHS_AuthorizationResult) resp->result);
            }
            break;
    }
    uv_timer_stop(timer);
}


bool IHS_ClientPriAuthorizationPubKey(IHS_Client *client, int euniverse, uint8_t *key, size_t *keyLen) {
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

static void IHS_AuthorizationTimer(uv_timer_t *handle, int status) {
    IHS_Client *client = handle->loop->data;
    IHS_AuthorizationState *state = handle->data;
    uint8_t pubKey[384];
    size_t pubKeyLen = sizeof(pubKey);
    IHS_ClientPriAuthorizationPubKey(client, state->host.euniverse, pubKey, &pubKeyLen);
    uint8_t deviceTokenBuf[128];
    ProtobufCBinaryData deviceToken = {.data=deviceTokenBuf, .len = sizeof(deviceTokenBuf)};
    IHS_ClientPriDeviceToken(client, deviceToken.data, &deviceToken.len);

    /* Initialize and serialize ticket */
    CMsgRemoteDeviceAuthorizationRequest__CKeyEscrowTicket ticket =
            CMSG_REMOTE_DEVICE_AUTHORIZATION_REQUEST__CKEY_ESCROW__TICKET__INIT;
    IHS_AuthorizationConfigureTicket(client, state, &ticket);
    uint8_t serTicket[2048];
    size_t serTicketLen = protobuf_c_message_pack(&ticket.base, serTicket);

    /* RSA encrypt ticket data */
    uint8_t encryptedTicket[2048];
    ProtobufCBinaryData encryptedRequest = {.data=encryptedTicket, .len = sizeof(encryptedTicket)};
    IHS_CryptoRSAEncrypt(serTicket, serTicketLen, pubKey, pubKeyLen, encryptedRequest.data,
                         &encryptedRequest.len);

    CMsgRemoteDeviceAuthorizationRequest request = {
            .device_name = state->deviceName,
            .device_token = deviceToken,
            .encrypted_request = encryptedRequest,
    };
    char ip[40];
    IHS_HostAddress address = state->host.address;
    uv_inet_ntop(address.ip.type, &address.ip.value, ip, sizeof(ip) - 1);
    IHS_ClientPriSend(client, ip, k_ERemoteDeviceAuthorizationRequest, (ProtobufCMessage *) &request);
}

static void IHS_AuthorizationCleanup(uv_handle_t *handle) {
    IHS_Client *client = handle->loop->data;
    client->timers.authorization = NULL;
    free(handle->data);
    free(handle);
}

static void IHS_AuthorizationConfigureTicket(IHS_Client *client, IHS_AuthorizationState *state,
                                             CMsgRemoteDeviceAuthorizationRequest__CKeyEscrowTicket *ticket) {
    ticket->has_password = true;
    ticket->password.len = strlen(state->pin);
    ticket->password.data = (uint8_t *) state->pin;

    ticket->has_identifier = true;
    ticket->identifier = client->deviceId;

    ticket->has_payload = true;
    ticket->payload.len = sizeof(client->secretKey);
    ticket->payload.data = client->secretKey;

    ticket->has_usage = true;
    ticket->usage = k_EKeyEscrowUsageStreamingDevice;

    ticket->device_name = client->deviceName;
}