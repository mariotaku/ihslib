#include <stdio.h>
#include "common.h"
#include "ihslib/client.h"


static void OnHostStatus(IHS_Client *client, IHS_HostInfo info);

void OnStreamingInProgress(IHS_Client *client);

void OnStreamingSuccess(IHS_Client *client, IHS_HostInfo host, uint16_t port, const uint8_t *sessionKey,
                        size_t sessionKeyLen);

void OnStreamingFailed(IHS_Client *client, IHS_StreamingResult result);

static bool AuthorizationStart = false;


int main(int argc, char *argv[]) {
    IHS_ClientConfig config = {deviceId, secretKey, deviceName};
    IHS_Client *client = IHS_ClientCreate(&config);
    IHS_ClientCallbacks callbacks = {
            .hostDiscovered = OnHostStatus,
            .streamingInProgress = OnStreamingInProgress,
            .streamingFailed = OnStreamingFailed,
            .streamingSuccess = OnStreamingSuccess,
    };
    IHS_ClientSetCallbacks(client, &callbacks);
    IHS_ClientDiscoveryBroadcast(client);
    IHS_ClientDestroy(client);
}

static void OnHostStatus(IHS_Client *client, IHS_HostInfo info) {
    if (AuthorizationStart) return;
    AuthorizationStart = true;
    printf("IHS_ClientStreamingRequest\n");
    IHS_StreamingRequest req = {
            .maxResolution = {1920, 1080},
            .streamingEnable = {true, true, true},
            .audioChannelCount = 2,
            .gamepadCount = 0
    };
    IHS_ClientStreamingRequest(client, &info, &req);
}


void OnStreamingInProgress(IHS_Client *client) {
}

void OnStreamingSuccess(IHS_Client *client, IHS_HostInfo host, uint16_t port, const uint8_t *sessionKey,
                        size_t sessionKeyLen) {
    printf("OnStreamingSuccess(sessionKey=\"");
    for (int i = 0; i < sessionKeyLen; i++) {
        printf("%02x", sessionKey[i]);
    }
    printf("\")\n");
    IHS_ClientStop(client);
}

void OnStreamingFailed(IHS_Client *client, IHS_StreamingResult result) {
    printf("OnStreamingFailed(result=%d)\n", result);
    IHS_ClientStop(client);
}