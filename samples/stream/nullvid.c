#include <stdio.h>
#include <string.h>
#include "stream.h"


static int Start(IHS_Session *session, const IHS_StreamVideoConfig *config, void *context) {
    return 0;
}


static void Stop(IHS_Session *session, void *context) {
}

static int Submit(IHS_Session *session, const uint8_t *data, size_t dataLen, uint16_t sequence,
                  IHS_StreamVideoFrameFlag flags, void *context) {
    return 0;
}


const IHS_StreamVideoCallbacks VideoCallbacks = {
        .start = Start,
        .submit = Submit,
        .stop = Stop,
};