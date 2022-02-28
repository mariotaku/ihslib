#include <stdio.h>
#include "common.h"
#include "ihslib/client.h"


static void OnHostStatus(IHS_Client *client, IHS_HostInfo info);

void OnStreamingInProgress(IHS_Client *client);

void OnAuthorizationSuccess(IHS_Client *client, uint64_t steamId);

void OnAuthorizationFailed(IHS_Client *client, IHS_AuthorizationResult result);

static bool AuthorizationStart = false;


int main(int argc, char *argv[]) {
    IHS_Client *client = IHS_ClientCreate(deviceId, secretKey, deviceName);
    IHS_ClientCallbacks callbacks = {
            .hostDiscovered = OnHostStatus,
            .authorizationInProgress = OnStreamingInProgress,
            .authorizationFailed = OnAuthorizationFailed,
            .authorizationSuccess = OnAuthorizationSuccess,
    };
    IHS_ClientSetCallbacks(client, &callbacks);
    IHS_ClientDiscoveryBroadcast(client);
    IHS_ClientDestroy(client);
}

static void OnHostStatus(IHS_Client *client, IHS_HostInfo info) {
    if (AuthorizationStart) return;
    AuthorizationStart = true;
    printf("IHS_ClientAuthorizationRequest");
    IHS_ClientAuthorizationRequest(client, &info, "1919");
}


void OnStreamingInProgress(IHS_Client *client) {
}

void OnAuthorizationSuccess(IHS_Client *client, uint64_t steamId) {
    printf("OnStreamingSuccess(steamId=%llu)\n", steamId);
    IHS_ClientStop(client);

}

void OnAuthorizationFailed(IHS_Client *client, IHS_AuthorizationResult result) {
    printf("OnStreamingFailed(result=%d)\n", result);
    IHS_ClientStop(client);
}