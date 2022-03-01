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

#include <stdlib.h>

#include "ihslib/session.h"
#include "ihslib/common.h"
#include "base.h"

struct IHS_Session {
    IHS_Base base;
};

typedef struct IHS_SessionState {
    IHS_SessionConfig config;
} IHS_SessionState;

static void SessionRecvCallback(uv_udp_t *handle, ssize_t nread, uv_buf_t buf, struct sockaddr *addr, unsigned flags);

IHS_Session *IHS_SessionCreate(const IHS_ClientConfig *config) {
    IHS_Session *session = malloc(sizeof(IHS_Session));
    IHS_BaseInit(&session->base, config, SessionRecvCallback, 0);
    return session;
}

void IHS_SessionStart(IHS_Session *session, const IHS_SessionConfig *config) {

}

void IHS_SessionDestroy(IHS_Session *session) {
    IHS_BaseFree(&session->base);
    free(session);
}

static void SessionRecvCallback(uv_udp_t *handle, ssize_t nread, uv_buf_t buf, struct sockaddr *addr, unsigned flags) {

}