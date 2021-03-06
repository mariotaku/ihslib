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

#include <stdio.h>
#include <string.h>
#include <signal.h>

#include "ihslib.h"
#include "stream.h"
#include "common.h"


static void InterruptHandler(int sig);

static void LogPrint(IHS_LogLevel level, const char *message);

static bool SetCursor(IHS_Session *session, uint64_t cursorId, void *context);

static void CursorImage(IHS_Session *session, const IHS_StreamInputCursorImage *image, void *context);

static bool Running = true;

IHS_Session *ActiveSession = NULL;

static IHS_StreamInputCallbacks InputCallbacks = {
        .setCursor = SetCursor,
        .cursorImage = CursorImage
};


int main(int argc, char *argv[]) {
    signal(SIGINT, InterruptHandler);

    IHS_SessionConfig sessionConfig;
    if (!RequestStream(&sessionConfig)) {
        return -1;
    }

    printf("Start Streaming, sessionKey[%u]=\"", sessionConfig.sessionKeyLen);
    for (int i = 0; i < sessionConfig.sessionKeyLen; i++) {
        printf("%02x", sessionConfig.sessionKey[i]);
    }
    printf("\"\n");

    IHS_Session *session = IHS_SessionCreate(&clientConfig, &sessionConfig);
    IHS_SessionSetLogFunction(session, LogPrint);
    IHS_SessionSetAudioCallbacks(session, &AudioCallbacks, NULL);
    IHS_SessionSetVideoCallbacks(session, &VideoCallbacks, NULL);
    IHS_SessionSetInputCallbacks(session, &InputCallbacks, NULL);
    if (!IHS_SessionConnect(session)) {
        fprintf(stderr, "Failed to start session\n");
        goto sessionExit;
    }
    ActiveSession = session;
    IHS_SessionRun(session);
    sessionExit:
    IHS_SessionDestroy(session);
}


static void InterruptHandler(int sig) {
    if (!ActiveSession) {
        signal(SIGINT, SIG_DFL);
        raise(SIGINT);
        return;
    }
    IHS_SessionDisconnect(ActiveSession);
}

static void LogPrint(IHS_LogLevel level, const char *message) {
    if (level >= IHS_BaseLogLevelWarn) {
        fprintf(stderr, "%s\n", message);
    } else {
        fprintf(stdout, "%s\n", message);
    }
}

static bool SetCursor(IHS_Session *session, uint64_t cursorId, void *context) {
    return false;
}

static void CursorImage(IHS_Session *session, const IHS_StreamInputCursorImage *image, void *context) {
    printf("Set cursor image: %d * %d\n", image->width, image->height);
}