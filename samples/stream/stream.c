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

#include <stdio.h>
#include <string.h>
#include <signal.h>

#include "ihslib.h"
#include "stream.h"
#include "common.h"

static void InterruptHandler(int sig);

static bool SetCursor(IHS_Session *session, uint64_t cursorId, void *context);

static void CursorImage(IHS_Session *session, const IHS_StreamInputCursorImage *image, void *context);

static void Configuring(IHS_Session *session, IHS_SessionConfig *config, void *context);

static bool Running = true;

IHS_Session *ActiveSession = NULL;

static IHS_StreamInputCallbacks InputCallbacks = {
        .setCursor = SetCursor,
        .cursorImage = CursorImage
};

static IHS_StreamSessionCallbacks SessionCallbacks = {
        .configuring = Configuring,
};

int main(int argc, char *argv[]) {
    signal(SIGINT, InterruptHandler);

    IHS_Init();
    VideoInit(argc, argv);

    IHS_SessionInfo sessionConfig;
    if (!RequestStream(&sessionConfig)) {
        return -1;
    }

    fprintf(stderr, "Start Streaming, sessionKey[%zu]=\"", sessionConfig.sessionKeyLen);
    for (int i = 0; i < sessionConfig.sessionKeyLen; i++) {
        fprintf(stderr, "%02x", sessionConfig.sessionKey[i]);
    }
    fprintf(stderr, "\"\n");

    IHS_Session *session = IHS_SessionCreate(&clientConfig, &sessionConfig);
    IHS_SessionSetLogFunction(session, LogPrint);
    IHS_SessionSetSessionCallbacks(session, &SessionCallbacks, NULL);
    IHS_SessionSetAudioCallbacks(session, &AudioCallbacks, NULL);
    IHS_SessionSetVideoCallbacks(session, &VideoCallbacks, NULL);
    IHS_SessionSetInputCallbacks(session, &InputCallbacks, NULL);
    if (!IHS_SessionConnect(session)) {
        fprintf(stderr, "Failed to start session\n");
        goto sessionExit;
    }
    ActiveSession = session;
    IHS_SessionThreadedJoin(session);
    sessionExit:
    ActiveSession = NULL;
    IHS_SessionDestroy(session);

    VideoDeinit();
    IHS_Quit();
    return 0;
}


static void InterruptHandler(int sig) {
    if (!ActiveSession) {
        signal(SIGINT, SIG_DFL);
        raise(SIGINT);
        return;
    }
    IHS_SessionDisconnect(ActiveSession);
}

void LogPrint(IHS_LogLevel level, const char *tag, const char *message) {
    const char *levelName = IHS_LogLevelName(level);
    switch (level) {
        case IHS_LogLevelInfo:
            fprintf(stderr, "[IHS.%s %s]\x1b[36m %s\x1b[0m\n", tag, levelName, message);
            break;
        case IHS_LogLevelWarn:
            fprintf(stderr, "[IHS.%s %s]\x1b[33m %s\x1b[0m\n", tag, levelName, message);
            break;
        case IHS_LogLevelError:
            fprintf(stderr, "[IHS.%s %s]\x1b[31m %s\x1b[0m\n", tag, levelName, message);
            break;
        case IHS_LogLevelFatal:
            fprintf(stderr, "[IHS.%s %s]\x1b[41m %s\x1b[0m\n", tag, levelName, message);
            break;
        default:
            fprintf(stderr, "[IHS.%s %s] %s\n", tag, levelName, message);
            break;
    }
}

static bool SetCursor(IHS_Session *session, uint64_t cursorId, void *context) {
    return true;
}

static void CursorImage(IHS_Session *session, const IHS_StreamInputCursorImage *image, void *context) {

}

static void Configuring(IHS_Session *session, IHS_SessionConfig *config, void *context) {
    config->enableAudio = false;
    config->enableHevc = false;
}