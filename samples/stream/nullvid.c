#include "stream.h"

static int Start(IHS_Session *session, const IHS_StreamVideoConfig *config, void *context) {
    return 0;
}


static void Stop(IHS_Session *session, void *context) {
}

static int Submit(IHS_Session *session, IHS_Buffer *data, IHS_StreamVideoFrameFlag flags, void *context) {

    return 0;
}


const IHS_StreamVideoCallbacks VideoCallbacks = {
        .start = Start,
        .submit = Submit,
        .stop = Stop,
};

void VideoInit(int argc, char *argv[]) {
}

void VideoDeinit() {
}