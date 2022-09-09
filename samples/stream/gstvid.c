#include <string.h>

#include "ihslib.h"

#include <gst/gst.h>
#include <gst/app/gstappsrc.h>

GstElement *pipeline;
GstAppSrc *source;

static int Start(IHS_Session *session, const IHS_StreamVideoConfig *config, void *context) {
    GError *error = NULL;
    pipeline = gst_parse_launch(
            "appsrc name=in ! h264parse ! avdec_h264 ! videoconvert ! autovideosink",
            &error);
    g_assert(pipeline != NULL);
    source = GST_APP_SRC(gst_bin_get_by_name(GST_BIN(pipeline), "in"));
    g_assert(source != NULL);
    gst_element_set_state(pipeline, GST_STATE_PLAYING);
    return 0;
}


static void Stop(IHS_Session *session, void *context) {
    gst_element_set_state(pipeline, GST_STATE_NULL);
    gst_object_unref(pipeline);
}

static int Submit(IHS_Session *session, const uint8_t *data, size_t dataLen, IHS_StreamVideoFrameFlag flags,
                  void *context) {
    g_assert(dataLen > 0);
    gpointer udata = g_memdup(data, dataLen);
    GstBuffer *buf = gst_buffer_new_wrapped(udata, dataLen);
    GstFlowReturn ret = gst_app_src_push_buffer(source, buf);
    g_assert(ret == GST_FLOW_OK);
    return 0;
}


const IHS_StreamVideoCallbacks VideoCallbacks = {
        .start = Start,
        .submit = Submit,
        .stop = Stop,
};