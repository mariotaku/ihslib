/*
 *  _____  _   _  _____  _  _  _
 * |_   _|| | | |/  ___|| |(_)| |     Steam
 *   | |  | |_| |\ `--. | | _ | |__     In-Home
 *   | |  |  _  | `--. \| || || '_ \      Streaming
 *  _| |_ | | | |/\__/ /| || || |_) |       Library
 *  \___/ \_| |_/\____/ |_||_||_.__/
 *
 * Copyright (c) 2026 Mariotaku <https://github.com/mariotaku>.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 */

#include <assert.h>
#include <string.h>
#include <stdio.h>

#include "session/session_pri.h"
#include "test_session.h"

static int activityCalls = 0;
static IHS_SessionActivityInfo lastCallbackInfo;

static void OnActivity(IHS_Session *session, const IHS_SessionActivityInfo *info, void *context) {
    (void) session;
    (void) context;
    activityCalls++;
    lastCallbackInfo = *info;
}

static const IHS_StreamSessionCallbacks sessionCallbacks = {
        .activity = OnActivity,
};

static void test_url_format(void) {
    char url[IHS_APP_IMAGE_URL_MAX];
    int len = IHS_AppImageURL(570, url, sizeof(url));
    assert(len > 0);
    assert((size_t) len < sizeof(url));
    assert(strcmp(url, "https://steamcdn-a.akamaihd.net/steam/apps/570/header.jpg") == 0);

    // The documented buffer size has to fit the widest app id there can be.
    char widest[IHS_APP_IMAGE_URL_MAX];
    int widestLen = IHS_AppImageURL(4294967295u, widest, sizeof(widest));
    assert(widestLen > 0);
    assert((size_t) widestLen < sizeof(widest));
    assert(strcmp(widest, "https://steamcdn-a.akamaihd.net/steam/apps/4294967295/header.jpg") == 0);
}

static void test_url_rejects_and_truncates(void) {
    char url[IHS_APP_IMAGE_URL_MAX];
    // App id 0 is not an app.
    assert(IHS_AppImageURL(0, url, sizeof(url)) == -1);
    assert(IHS_AppImageURL(570, NULL, sizeof(url)) == -1);
    // Measuring with a NULL destination and zero length is allowed, like snprintf.
    assert(IHS_AppImageURL(570, NULL, 0) == 57);

    // snprintf semantics: terminated, and the return value reports what would have fit.
    char small[16];
    int len = IHS_AppImageURL(570, small, sizeof(small));
    assert(len == 57);
    assert((size_t) len >= sizeof(small));
    assert(small[sizeof(small) - 1] == '\0');
    assert(strncmp(small, "https://steamcd", 15) == 0);
}

static void test_activity_round_trip(void) {
    IHS_Init();
    IHS_Session *session = IHS_TestSessionCreate();
    IHS_SessionSetSessionCallbacks(session, &sessionCallbacks, NULL);

    // Nothing reported yet.
    IHS_SessionActivityInfo info;
    memset(&info, 0xCC, sizeof(info));
    assert(!IHS_SessionGetActivity(session, &info));
    // A failed get must not touch the caller's buffer.
    assert(info.appId == 0xCCCCCCCCu);
    uint32_t appId = 12345;
    assert(!IHS_SessionGetLastGameAppID(session, &appId));
    assert(appId == 12345);

    IHS_SessionActivityInfo game = {
            .activity = IHS_SessionActivityGame,
            .appId = 570,
            .gameId = 570,
    };
    strcpy(game.gameName, "Dota 2");
    IHS_SessionSetActivity(session, &game);

    assert(activityCalls == 1);
    assert(lastCallbackInfo.appId == 570);
    assert(strcmp(lastCallbackInfo.gameName, "Dota 2") == 0);

    assert(IHS_SessionGetActivity(session, &info));
    assert(info.activity == IHS_SessionActivityGame);
    assert(info.appId == 570);
    assert(info.gameId == 570);
    assert(strcmp(info.gameName, "Dota 2") == 0);

    assert(IHS_SessionGetLastGameAppID(session, &appId));
    assert(appId == 570);

    // Switching to the desktop keeps an activity, but there is no game to look up an image for.
    IHS_SessionActivityInfo desktop = {.activity = IHS_SessionActivityDesktop};
    IHS_SessionSetActivity(session, &desktop);
    assert(activityCalls == 2);
    assert(IHS_SessionGetActivity(session, &info));
    assert(info.activity == IHS_SessionActivityDesktop);
    assert(info.appId == 0);
    appId = 999;
    assert(!IHS_SessionGetLastGameAppID(session, &appId));
    assert(appId == 999);

    IHS_SessionDestroy(session);
    IHS_Quit();
}

int main(void) {
    test_url_format();
    test_url_rejects_and_truncates();
    test_activity_round_trip();
    printf("activity tests OK\n");
    return 0;
}
