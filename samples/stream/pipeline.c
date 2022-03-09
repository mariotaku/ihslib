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

#include "ihslib.h"
#include "stream.h"

#include <gst/app/gstappsrc.h>
#include <stdio.h>
#include <assert.h>

static void OnAudioStart(void *context, const IHS_StreamAudioConfig *config);

static void OnAudioReceived(void *context, const uint8_t *data, size_t dataLen);

static void OnAudioStop(void *context);

static void OnVideoStart(void *context, const IHS_StreamVideoConfig *config);

static void OnVideoReceived(void *context, const uint8_t *data, size_t dataLen, uint16_t sequence, uint8_t flags);

static void OnVideoStop(void *context);

const unsigned char zeroes[] = {
        0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0,
};

const IHS_StreamAudioCallbacks AudioCallbacks = {
        .start = OnAudioStart,
        .received = OnAudioReceived,
        .stop = OnAudioStop,
};

const IHS_StreamVideoCallbacks VideoCallbacks = {
        .start = OnVideoStart,
        .received = OnVideoReceived,
        .stop = OnVideoStop,
};

static GstElement *audioPipeline = NULL, *videoPipeline = NULL;
static GstElement *audioSrc = NULL, *videoSrc = NULL;

static void OnAudioStart(void *context, const IHS_StreamAudioConfig *config) {
    printf("OnAudioStart\n");
    fflush(stdout);
    GError *error = NULL;
    audioPipeline = gst_parse_launch(
            "appsrc is-live=true name=audsrc max-latency=960 min-latency=1 ! opusparse ! opusdec ! audioconvert ! alsasink",
            &error);
    if (error) {
        fprintf(stderr, "Pipeline error: %s\n", error->message);
        abort();
    }
    assert(audioPipeline != NULL);
    audioSrc = gst_bin_get_by_name(GST_BIN(audioPipeline), "audsrc");
    assert(audioSrc != NULL);
    GstCaps *caps = gst_caps_new_simple("audio/x-opus",
                                        "channels", G_TYPE_INT, config->channels,
                                        "rate", G_TYPE_INT, config->frequency,
                                        "channel-mapping-family", G_TYPE_INT, 0,
                                        NULL);
    gst_app_src_set_caps(GST_APP_SRC(audioSrc), caps);
    gst_app_src_set_duration(GST_APP_SRC(audioSrc), GST_CLOCK_TIME_NONE);
    gst_app_src_set_stream_type(GST_APP_SRC(audioSrc), GST_APP_STREAM_TYPE_STREAM);

    gst_element_set_state(audioPipeline, GST_STATE_PLAYING);
    printf("Start audio pipeline...\n");
    fflush(stdout);
}

static void OnAudioReceived(void *context, const uint8_t *data, size_t dataLen) {
    void *bufData = malloc(dataLen);
    memcpy(bufData, data, dataLen);
    GstBuffer *buffer = gst_buffer_new_wrapped(bufData, dataLen);
    GstFlowReturn ret = gst_app_src_push_buffer(GST_APP_SRC(audioSrc), buffer);
    if (ret != GST_FLOW_OK) {
        fprintf(stderr, "Push audio buffer error: %s\n", gst_flow_get_name(ret));
    }
}

static void OnAudioStop(void *context) {
    printf("OnAudioStop\n");
    fflush(stdout);
    if (!audioPipeline) return;
    gst_element_set_state(audioPipeline, GST_STATE_NULL);
    gst_object_unref(audioPipeline);
    audioPipeline = NULL;
}

static int numOfFrames = 0;
static FILE *file = NULL;

static void OnVideoStart(void *context, const IHS_StreamVideoConfig *config) {
    assert(config->codec == IHS_StreamVideoCodecH264);
    printf("OnVideoStart\n");
    numOfFrames = 0;
    file = NULL;
    GError *error = NULL;
    videoPipeline = gst_parse_launch(
            "appsrc is-live=true name=vidsrc ! h264parse config-interval=-1 ! avdec_h264 ! videoconvert ! autovideosink",
            &error);
    if (error) {
        fprintf(stderr, "Pipeline error: %s\n", error->message);
        abort();
    }
    assert(videoPipeline != NULL);
    videoSrc = gst_bin_get_by_name(GST_BIN(videoPipeline), "vidsrc");
    assert(videoSrc != NULL);
//    gst_app_src_set_duration(GST_APP_SRC(videoSrc), GST_CLOCK_TIME_NONE);
    gst_app_src_set_stream_type(GST_APP_SRC(videoSrc), GST_APP_STREAM_TYPE_STREAM);

    gst_element_set_state(videoPipeline, GST_STATE_PLAYING);
    printf("Start video pipeline...\n");
    fflush(stdout);
}

static void OnVideoReceived(void *context, const uint8_t *data, size_t dataLen, uint16_t sequence, uint8_t flags) {
    void *bufData = malloc(dataLen);
    memcpy(bufData, data, dataLen);

    uint8_t nal_ref_idc = (data[4] >> 5) & 0x3;
    uint8_t nal_unit_type = data[4] & 0x1F;

//    printf("OnVideoReceived(data[%05u]=\"", dataLen);
//    for (size_t i = 0, j = dataLen < 16 ? dataLen : 16; i < j; i++) {
//        printf(i > 0 ? " %02x" : "%02x", data[i]);
//    }
//    printf("\")\n");

//    if (numOfFrames < 10000) {
//        if (!file) {
//            file = fopen("/tmp/ihscap.h264", "wb");
//        }
//        fwrite(data, dataLen, 1, file);
//        fwrite(zeroes, 1, sizeof(zeroes), file);
//        numOfFrames++;
//    } else if (file) {
//        fflush(file);
//        fclose(file);
//        file = NULL;
//        if (ActiveSession) {
//            IHS_SessionDisconnect(ActiveSession);
//        }
//    }

    GstBuffer *buffer = gst_buffer_new_wrapped(bufData, dataLen);
    gst_buffer_set_flags(buffer, GST_BUFFER_FLAG_LIVE);
    GstFlowReturn ret = gst_app_src_push_buffer(GST_APP_SRC(videoSrc), buffer);
    if (ret != GST_FLOW_OK) {
        fprintf(stderr, "Push video buffer error: %s\n", gst_flow_get_name(ret));
    }
}

static void OnVideoStop(void *context) {
    printf("OnVideoStop\n");
    fflush(stdout);
    if (!videoPipeline) return;
    gst_element_set_state(videoPipeline, GST_STATE_NULL);
    gst_object_unref(videoPipeline);
    videoPipeline = NULL;
}