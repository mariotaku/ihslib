#include <stdio.h>
#include "ihslib.h"
#include "common.h"

static void OnHostStatus(IHS_Client *client, IHS_HostInfo info);

int main(int argc, char *argv[]) {
    IHS_Client *client = IHS_ClientCreate(deviceId, secretKey, deviceName);
    IHS_ClientCallbacks callbacks = {
            .hostDiscovered = OnHostStatus
    };
    IHS_ClientSetCallbacks(client, &callbacks);
    IHS_ClientDiscoveryBroadcast(client);
    IHS_ClientDestroy(client);
}

static void OnHostStatus(IHS_Client *client, IHS_HostInfo info) {
    printf("Found device: %s, port: %d, euniverse: %d\n",
           info.hostname, info.address.port, info.euniverse);
}