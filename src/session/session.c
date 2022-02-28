#include <stdlib.h>

#include "ihslib/session.h"
#include "ihslib/common.h"
#include "base.h"

struct IHS_Session {
    IHS_Base base;
};

static void SessionRecvCallback(uv_udp_t *handle, ssize_t nread, uv_buf_t buf, struct sockaddr *addr, unsigned flags);

IHS_Session *IHS_SessionCreate(const IHS_ClientConfig *config) {
    IHS_Session *session = malloc(sizeof(IHS_Session));
    IHS_BaseInit(&session->base, config, SessionRecvCallback, 0);
    return session;
}

void IHS_SessionStart(IHS_SessionConfig config) {

}

void IHS_SessionDestroy(IHS_Session *session) {
    IHS_BaseFree(&session->base);
    free(session);
}

static void SessionRecvCallback(uv_udp_t *handle, ssize_t nread, uv_buf_t buf, struct sockaddr *addr, unsigned flags) {

}