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

#include <stdio.h>
#include <string.h>

#include "session_pri.h"

/**
 * The host that CAppImages::DownloadImage @ 0x175e0c fetches from. Unauthenticated: the app id is
 * the only input, there is no session, key or token involved.
 */
#define APP_IMAGE_URL_FORMAT "https://steamcdn-a.akamaihd.net/steam/apps/%u/header.jpg"

void IHS_SessionSetActivity(IHS_Session *session, const IHS_SessionActivityInfo *info) {
    IHS_BaseLock(&session->base);
    session->activity.info = *info;
    session->activity.valid = true;
    const IHS_StreamSessionCallbacks *callbacks = session->callbacks.session;
    void *context = session->callbackContexts.session;
    IHS_BaseUnlock(&session->base);
    // Called outside the lock: the callback runs application code, which is free to call back into
    // the session (IHS_SessionGetActivity being the obvious one).
    if (callbacks != NULL && callbacks->activity != NULL) {
        callbacks->activity(session, info, context);
    }
}

bool IHS_SessionGetActivity(IHS_Session *session, IHS_SessionActivityInfo *info) {
    IHS_BaseLock(&session->base);
    bool valid = session->activity.valid;
    if (valid && info != NULL) {
        *info = session->activity.info;
    }
    IHS_BaseUnlock(&session->base);
    return valid;
}

bool IHS_SessionGetLastGameAppID(IHS_Session *session, uint32_t *appId) {
    IHS_SessionActivityInfo info;
    if (!IHS_SessionGetActivity(session, &info)) {
        return false;
    }
    // An activity without an app id is the desktop, music, or a game the host declined to name;
    // none of those are something a caller can look up an image for.
    if (info.appId == 0) {
        return false;
    }
    if (appId != NULL) {
        *appId = info.appId;
    }
    return true;
}

int IHS_AppImageURL(uint32_t appId, char *dest, size_t destLen) {
    if (appId == 0) {
        return -1;
    }
    if (dest == NULL && destLen != 0) {
        return -1;
    }
    return snprintf(dest, destLen, APP_IMAGE_URL_FORMAT, appId);
}
