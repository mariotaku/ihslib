#include <stdio.h>
#include "common.h"
#include "ihslib/client.h"


static void OnHostStatus(IHS_Client *client, IHS_HostInfo info);

void OnAuthorizationInProgress(IHS_Client *client);

void OnAuthorizationSuccess(IHS_Client *client, uint64_t steamId);

void OnAuthorizationFailed(IHS_Client *client, IHS_AuthorizationResult result);

static bool AuthorizationStart = false;


int main(int argc, char *argv[]) {
    IHS_Client *client = IHS_ClientCreate(deviceId, secretKey, deviceName);
    IHS_ClientCallbacks callbacks = {
            .hostDiscovered = OnHostStatus,
            .authorizationInProgress = OnAuthorizationInProgress
    };
    IHS_ClientSetCallbacks(client, &callbacks);
    IHS_ClientDiscoveryBroadcast(client);
    IHS_ClientDestroy(client);
}

static void OnHostStatus(IHS_Client *client, IHS_HostInfo info) {
    if (AuthorizationStart) return;
    AuthorizationStart = true;
    IHS_ClientStartAuthorization(client, &info, "1919");
}


void OnAuthorizationInProgress(IHS_Client *client) {
    printf("OnAuthorizationInProgress()\n");
}

void OnAuthorizationSuccess(IHS_Client *client, uint64_t steamId) {

}

void OnAuthorizationFailed(IHS_Client *client, IHS_AuthorizationResult result) {
    printf("OnAuthorizationFailed(result=%d)\n", result);
}