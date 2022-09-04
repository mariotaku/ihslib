#include "stream.h"

int Start(IHS_Session *session, const IHS_StreamAudioConfig *config, void *context) {
    return 0;
}

int Submit(IHS_Session *session, const uint8_t *data, size_t dataLen, void *context) {
    return 0;
}

void Stop(IHS_Session *session, void *context) {

}

const IHS_StreamAudioCallbacks AudioCallbacks = {
        .start = Start,
        .stop = Stop,
        .submit = Submit
};