#include <malloc.h>
#include <string.h>
#include <stdlib.h>
#include "client_pri.h"
#include "ihslib/crypto.h"

typedef struct IHS_StreamingState {
    IHS_HostInfo host;
    IHS_StreamingRequest request;
    int32_t requestId;
} IHS_StreamingState;

static void StreamingRequestTimer(uv_timer_t *handle, int status);

static void StreamingRequestCleanup(uv_handle_t *handle);

bool IHS_ClientStreamingRequest(IHS_Client *client, const IHS_HostInfo *host, const IHS_StreamingRequest *request) {
    if (client->taskHandles.streaming) {
        return false;
    }
    uv_timer_t *timer = malloc(sizeof(uv_timer_t));
    uv_timer_init(client->loop, timer);
    timer->close_cb = StreamingRequestCleanup;
    IHS_StreamingState *state = malloc(sizeof(IHS_StreamingState));
    state->host = *host;
    state->request = *request;
    state->requestId = rand(); // NOLINT(cert-msc50-cpp)
    timer->data = state;
    IHS_PRIV_ClientLock(client);
    client->taskHandles.streaming = timer;
    IHS_PRIV_ClientUnlock(client);
    uv_timer_start(timer, StreamingRequestTimer, 0, 10000);
    return true;
}

void IHS_PRIV_ClientStreamingCallback(IHS_Client *client, IHS_HostIP ip, CMsgRemoteClientBroadcastHeader *header,
                                      ProtobufCMessage *message) {
    switch (header->msg_type) {
        case k_ERemoteDeviceProofRequest: {
            IHS_StreamingState *state = client->taskHandles.streaming->data;
            CMsgRemoteDeviceProofRequest *request = (CMsgRemoteDeviceProofRequest *) message;
            if (request->request_id != state->requestId) return;
            CMsgRemoteDeviceProofResponse response = CMSG_REMOTE_DEVICE_PROOF_RESPONSE__INIT;
            response.has_request_id = true;
            response.request_id = request->request_id;

            uint8_t encrypted[1024];
            response.response.data = encrypted;
            response.response.len = sizeof(encrypted);
            // TODO: check return code
            IHS_CryptoSymmetricEncrypt(request->challenge.data, request->challenge.len,
                                       client->secretKey, sizeof(client->secretKey),
                                       response.response.data, &response.response.len);

            IHS_PRIV_ClientSend(client, state->host.address, k_ERemoteDeviceProofResponse,
                                (ProtobufCMessage *) &response);
            break;
        }
        case k_ERemoteDeviceStreamingResponse: {
            IHS_StreamingState *state = client->taskHandles.streaming->data;
            CMsgRemoteDeviceStreamingResponse *response = (CMsgRemoteDeviceStreamingResponse *) message;
            if (response->request_id != state->requestId) return;
            switch (response->result) {
                case k_ERemoteDeviceStreamingSuccess:
                    if (client->callbacks.streamingSuccess) {
                        ProtobufCBinaryData enc = response->encrypted_session_key;
                        uint8_t key[128];
                        size_t keyLen = sizeof(key);
                        IHS_CryptoSymmetricDecrypt(enc.data, enc.len, client->secretKey, sizeof(client->secretKey),
                                                   key, &keyLen);
                        client->callbacks.streamingSuccess(client, key, keyLen);
                    }
                    break;
                case k_ERemoteDeviceStreamingInProgress:
                    if (client->callbacks.streamingInProgress) {
                        client->callbacks.streamingInProgress(client);
                    }
                    break;
                default:
                    if (client->callbacks.streamingFailed) {
                        client->callbacks.streamingFailed(client, (IHS_StreamingResult) response->result);
                    }
                    break;
            }
            break;
        }
        default:
            break;
    }
}

static void StreamingRequestTimer(uv_timer_t *handle, int status) {
    IHS_Client *client = handle->loop->data;
    IHS_StreamingState *state = handle->data;
    IHS_HostInfo host = state->host;
    IHS_StreamingRequest request = state->request;

    CMsgRemoteDeviceStreamingRequest message = CMSG_REMOTE_DEVICE_STREAMING_REQUEST__INIT;
    message.request_id = state->requestId;

    message.has_client_id = true;
    message.client_id = host.clientId;

    message.has_pin = true;
    message.pin.len = strnlen(request.pin, sizeof(request.pin));
    message.pin.data = (uint8_t *) strndup(request.pin, sizeof(request.pin));

    message.has_maximum_resolution_x = request.maxResolution.x > 0;
    message.maximum_resolution_x = request.maxResolution.x;
    message.has_maximum_resolution_y = request.maxResolution.y > 0;
    message.maximum_resolution_y = request.maxResolution.y;

    message.has_enable_audio_streaming = true;
    message.enable_audio_streaming = request.streamingEnable.audio;

    message.has_enable_video_streaming = true;
    message.enable_video_streaming = request.streamingEnable.video;

    message.has_enable_input_streaming = true;
    message.enable_input_streaming = request.streamingEnable.input;

    message.has_audio_channel_count = true;
    message.audio_channel_count = request.audioChannelCount;

    message.has_gamepad_count = request.gamepadCount >= 0;
    message.gamepad_count = request.gamepadCount;

    message.has_restricted = true;
    message.restricted = false;

    message.has_stream_interface = true;
    message.stream_interface = k_EStreamInterfaceDefault;

    message.has_stream_desktop = true;
    message.stream_desktop = true;

    message.has_form_factor = true;
    message.form_factor = k_EStreamDeviceFormFactorTV;

    message.has_device_token = true;
    message.device_token.data = client->deviceToken;
    message.device_token.len = sizeof(client->deviceToken);

    message.n_supported_transport = 1;
    EStreamTransport transports[] = {k_EStreamTransportUDP};
    message.supported_transport = transports;

    IHS_PRIV_ClientSend(client, host.address, k_ERemoteDeviceStreamingRequest,
                        (ProtobufCMessage *) &message);
}

static void StreamingRequestCleanup(uv_handle_t *handle) {
    IHS_Client *client = handle->loop->data;
    IHS_PRIV_ClientLock(client);
    client->taskHandles.streaming = NULL;
    IHS_PRIV_ClientUnlock(client);
    free(handle->data);
    free(handle);
}