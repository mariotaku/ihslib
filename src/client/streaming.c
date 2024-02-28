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

#include <string.h>
#include <stdlib.h>
#include "client_pri.h"
#include "crypto.h"

typedef struct IHS_StreamingState {
    IHS_Client *client;
    IHS_HostInfo host;
    IHS_StreamingRequest request;
    uint32_t requestId;
    ERemoteClientBroadcastMsg lastMsgType;
    uint64_t lastMsgTime;
} IHS_StreamingState;

static uint64_t StreamingRequestTimer(int runCount, void *context);

static void StreamingRequestCleanup(void *context);

bool IHS_ClientStreamingRequest(IHS_Client *client, const IHS_HostInfo *host, const IHS_StreamingRequest *request) {
    if (client->taskHandles.streaming) {
        return false;
    }
    IHS_ClientLog(client, IHS_LogLevelInfo, "Client", "Begin sending streaming request to host %s", host->hostname);
    IHS_StreamingState *state = malloc(sizeof(IHS_StreamingState));
    state->client = client;
    state->host = *host;
    state->request = *request;
    state->requestId = IHS_CryptoRandomUInt32();
    state->lastMsgType = k_ERemoteClientBroadcastMsgDiscovery;
    state->lastMsgTime = IHS_TimerNow();
    IHS_BaseLock(&client->base);
    client->taskHandles.streaming = IHS_TimerTaskStart(client->timers, StreamingRequestTimer, StreamingRequestCleanup,
                                                       0, state);
    IHS_BaseUnlock(&client->base);
    return true;
}

void IHS_ClientStreamingCallback(IHS_Client *client, const IHS_SocketAddress *address,
                                 CMsgRemoteClientBroadcastHeader *header, ProtobufCMessage *message) {
    IHS_UNUSED(address);
    IHS_TimerTask *timer = client->taskHandles.streaming;
    if (!timer) return;
    IHS_StreamingState *state = IHS_TimerTaskGetContext(timer);
    state->lastMsgType = header->msg_type;
    state->lastMsgTime = IHS_TimerNow();
    switch (header->msg_type) {
        case k_ERemoteDeviceProofRequest: {
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
                                       client->base.secretKey, sizeof(client->base.secretKey),
                                       response.response.data, &response.response.len);

            IHS_ClientSend(client, state->host.address, k_ERemoteDeviceProofResponse,
                           (ProtobufCMessage *) &response);
            break;
        }
        case k_ERemoteDeviceStreamingResponse: {
            CMsgRemoteDeviceStreamingResponse *response = (CMsgRemoteDeviceStreamingResponse *) message;
            if (response->request_id != state->requestId) return;
            switch (response->result) {
                case k_ERemoteDeviceStreamingInProgress:
                    IHS_ClientLog(client, IHS_LogLevelDebug, "Client", "Streaming request in progress: host %s",
                                  state->host.hostname);
                    if (client->callbacks.streaming && client->callbacks.streaming->progress) {
                        client->callbacks.streaming->progress(client, &state->host, client->callbackContexts.streaming);
                    }
                    return;
                case k_ERemoteDeviceStreamingSuccess:
                    IHS_ClientLog(client, IHS_LogLevelDebug, "Client", "Streaming request succeeded: host %s",
                                  state->host.hostname);
                    if (client->callbacks.streaming && client->callbacks.streaming->success) {
                        ProtobufCBinaryData enc = response->encrypted_session_key;
                        uint8_t key[128];
                        size_t keyLen = sizeof(key);
                        IHS_CryptoSymmetricDecrypt(enc.data, enc.len, client->base.secretKey,
                                                   sizeof(client->base.secretKey), key, &keyLen);
                        IHS_SocketAddress streamingAddress = {state->host.address.ip, response->port};
                        client->callbacks.streaming->success(client, &state->host, &streamingAddress, key, keyLen,
                                                             client->callbackContexts.streaming);
                    }
                    break;
                default:
                    IHS_ClientLog(client, IHS_LogLevelWarn, "Client", "Streaming request failed: host %s",
                                  state->host.hostname);
                    if (client->callbacks.streaming && client->callbacks.streaming->failed) {
                        IHS_StreamingResult result = (IHS_StreamingResult) response->result;
                        client->callbacks.streaming->failed(client, &state->host, result,
                                                            client->callbackContexts.streaming);
                    }
                    break;
            }
            IHS_TimerTaskStop(timer);
            break;
        }
        default:
            break;
    }
}

static uint64_t StreamingRequestTimer(int runCount, void *context) {
    (void) runCount;
    IHS_StreamingState *state = context;
    IHS_Client *client = state->client;
    if ((IHS_TimerNow() - state->lastMsgTime) > 15000) {
        IHS_ClientLog(client, IHS_LogLevelWarn, "Client", "Streaming request timed out: host %s",
                      state->host.hostname);
        if (client->callbacks.streaming && client->callbacks.streaming->failed) {
            client->callbacks.streaming->failed(client, &state->host, IHS_StreamingTimeout,
                                                client->callbackContexts.streaming);
        }
        return 0;
    }
    IHS_HostInfo host = state->host;
    IHS_StreamingRequest request = state->request;

    CMsgRemoteDeviceStreamingRequest message = CMSG_REMOTE_DEVICE_STREAMING_REQUEST__INIT;
    message.request_id = state->requestId;

    message.has_client_id = true;
    message.client_id = host.clientId;

    message.has_pin = true;
    message.pin.len = strnlen(request.pin, sizeof(request.pin));
    message.pin.data = (uint8_t *) request.pin;

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

    message.has_restricted = true;
    message.restricted = false;

    message.has_stream_interface = true;
    message.stream_interface = (EStreamInterface) request.streamingInterface;

    message.has_stream_desktop = true;
    message.stream_desktop = true;

    message.has_form_factor = true;
    message.form_factor = k_EStreamDeviceFormFactorTV;

    message.has_device_token = true;
    message.device_token.data = client->base.deviceToken;
    message.device_token.len = sizeof(client->base.deviceToken);

    message.n_supported_transport = 1;
    EStreamTransport transports[] = {k_EStreamTransportUDP};
    message.supported_transport = transports;

    IHS_ClientLog(client, IHS_LogLevelDebug, "Client", "Sending streaming request packet to host %s", host.hostname);
    IHS_ClientSend(client, host.address, k_ERemoteDeviceStreamingRequest, (ProtobufCMessage *) &message);
    return 3000;
}

static void StreamingRequestCleanup(void *context) {
    IHS_StreamingState *state = context;
    IHS_Client *client = state->client;
    IHS_BaseLock(&client->base);
    client->taskHandles.streaming = NULL;
    IHS_BaseUnlock(&client->base);
    free(context);
}