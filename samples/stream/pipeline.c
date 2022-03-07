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

static void OnVideoReceived(void *context, const uint8_t *data, size_t dataLen, uint16_t sequence);

static void OnVideoStop(void *context);

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
    audioPipeline = gst_parse_launch("appsrc name=audsrc ! opusdec ! alsasink", &error);
    if (error) {
        fprintf(stderr, "Pipeline error: %s\n", error->message);
        abort();
    }
    assert(audioPipeline != NULL);
    audioSrc = gst_bin_get_by_name(GST_BIN(audioPipeline), "audsrc");
    assert(audioSrc != NULL);
    GstCaps *caps = gst_caps_new_simple("audio/x-opus",
                                        "channels", G_TYPE_INT, config->channels,
                                        NULL);
    gst_app_src_set_caps(GST_APP_SRC(audioSrc), caps);
    gst_app_src_set_duration(GST_APP_SRC(audioSrc), GST_CLOCK_TIME_NONE);
    gst_app_src_set_stream_type(GST_APP_SRC(audioSrc), GST_APP_STREAM_TYPE_STREAM);

    gst_element_set_state(audioPipeline, GST_STATE_PLAYING);
    printf("Start audio pipeline...\n");
    fflush(stdout);
}

static void OnAudioReceived(void *context, const uint8_t *data, size_t dataLen) {
//    printf("OnAudioReceived\n");
//    GstBuffer *buffer = gst_buffer_new_wrapped((gpointer) data, dataLen);
//    gst_app_src_push_buffer(GST_APP_SRC(audioSrc), buffer);
}

static void OnAudioStop(void *context) {
    printf("OnAudioStop\n");
    fflush(stdout);
    if (!audioPipeline) return;
    gst_element_set_state(audioPipeline, GST_STATE_NULL);
    gst_object_unref(audioPipeline);
}

static void OnVideoStart(void *context, const IHS_StreamVideoConfig *config) {
    printf("OnVideoStart\n");
}

static void OnVideoReceived(void *context, const uint8_t *data, size_t dataLen, uint16_t sequence) {
//    printf("OnVideoReceived\n");
}

static void OnVideoStop(void *context) {
    printf("OnVideoStop\n");
}