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
    static uint16_t lastSequence = 0;
    int diff = sequence - lastSequence;
    if (diff > INT16_MIN && diff > 1) {
        fprintf(stderr, "Unexpected frame sequence: %u\n", sequence);
    }
    lastSequence = sequence;
    return 0;
}


const IHS_StreamVideoCallbacks VideoCallbacks = {
        .start = Start,
        .submit = Submit,
        .stop = Stop,
};