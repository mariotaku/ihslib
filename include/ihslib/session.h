#pragma once

#include "common.h"

typedef struct IHS_Session IHS_Session;

typedef struct IHS_SessionConfig {

} IHS_SessionConfig;

IHS_Session *IHS_SessionCreate(const IHS_ClientConfig *config);

void IHS_SessionStart(IHS_SessionConfig config);

void IHS_SessionDestroy(IHS_Session *session);