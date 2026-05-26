/*
 *  _____  _   _  _____  _  _  _
 * |_   _|| | | |/  ___|| |(_)| |     Steam
 *   | |  | |_| |\ `--. | | _ | |__     In-Home
 *   | |  |  _  | `--. \| || || '_ \      Streaming
 *  _| |_ | | | |/\__/ /| || || |_) |       Library
 *  \___/ \_| |_/\____/ |_||_||_.__/
 *
 * Copyright (c) 2022 Mariotaku <https://github.com/mariotaku>.
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

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "test_session.h"
#include "ihslib.h"
#include "ihs_buffer.h"
#include "ihs_buffer_ext.h"
#include "session/channels/ch_control.h"
#include "session/channels/channel.h"
#include "protobuf/remoteplay.pb-c.h"

typedef struct callback_log_t {
    int set_capture_size_calls;
    int set_target_framerate_calls;
    int set_target_bitrate_calls;
    int set_quality_override_calls;
    int set_bitrate_override_calls;

    int capture_width, capture_height;
    uint32_t framerate_num, framerate_denom, framerate_reasons;
    int32_t target_bitrate;
    int32_t quality_override;
    int32_t bitrate_override;
} callback_log_t;

static int on_set_capture_size(IHS_Session *session, int width, int height, void *context) {
    (void) session;
    callback_log_t *log = context;
    log->set_capture_size_calls++;
    log->capture_width = width;
    log->capture_height = height;
    return 0;
}

static void on_set_target_framerate(IHS_Session *session, uint32_t num, uint32_t denom, uint32_t reasons,
                                    void *context) {
    (void) session;
    callback_log_t *log = context;
    log->set_target_framerate_calls++;
    log->framerate_num = num;
    log->framerate_denom = denom;
    log->framerate_reasons = reasons;
}

static void on_set_target_bitrate(IHS_Session *session, int32_t bitrate, void *context) {
    (void) session;
    callback_log_t *log = context;
    log->set_target_bitrate_calls++;
    log->target_bitrate = bitrate;
}

static void on_set_quality_override(IHS_Session *session, int32_t value, void *context) {
    (void) session;
    callback_log_t *log = context;
    log->set_quality_override_calls++;
    log->quality_override = value;
}

static void on_set_bitrate_override(IHS_Session *session, int32_t value, void *context) {
    (void) session;
    callback_log_t *log = context;
    log->set_bitrate_override_calls++;
    log->bitrate_override = value;
}

static const IHS_StreamVideoCallbacks callbacks = {
        .setCaptureSize = on_set_capture_size,
        .setTargetFramerate = on_set_target_framerate,
        .setTargetBitrate = on_set_target_bitrate,
        .setQualityOverride = on_set_quality_override,
        .setBitrateOverride = on_set_bitrate_override,
};

/* Pack a protobuf into an owned IHS_Buffer and dispatch it through the video
 * control handler. The caller owns the buffer's underlying storage; we clear
 * it here. */
static void dispatch_video_msg(IHS_SessionChannel *channel, EStreamControlMessage type,
                               const ProtobufCMessage *message) {
    size_t size = protobuf_c_message_get_packed_size(message);
    /* Buffer needs a non-zero capacity even for empty payloads (the empty-payload
     * HighResCapture messages have size 0). */
    IHS_Buffer buf;
    IHS_BufferInit(&buf, size > 0 ? size : 1, size > 0 ? size : 1);
    if (size > 0) {
        IHS_BufferAppendMessage(&buf, message);
    }
    IHS_SessionChannelControlOnVideo(channel, type, &buf, NULL);
    IHS_BufferClear(&buf, true);
}

int main(void) {
    IHS_Init();
    IHS_Session *session = IHS_TestSessionCreate();
    assert(session != NULL);
    callback_log_t log;
    memset(&log, 0, sizeof(log));
    IHS_SessionSetVideoCallbacks(session, &callbacks, &log);

    /* IHS_SessionChannelControlOnVideo only touches channel->session, so a
     * synthetic stack channel is sufficient. */
    IHS_SessionChannel channel = {.session = session};

    /* SetCaptureSize — already worked before this change; included as a
     * regression baseline. width/height are protobuf `optional`, so we have
     * to set the has_ flags or they won't be serialized and the handler
     * will see zeroes. */
    {
        CSetCaptureSizeMsg msg = CSET_CAPTURE_SIZE_MSG__INIT;
        msg.has_width = 1;
        msg.width = 1920;
        msg.has_height = 1;
        msg.height = 1080;
        dispatch_video_msg(&channel, k_EStreamControlSetCaptureSize, (const ProtobufCMessage *) &msg);
        assert(log.set_capture_size_calls == 1);
        assert(log.capture_width == 1920);
        assert(log.capture_height == 1080);
    }

    /* SetTargetFramerate — fraction populated, reasons bitmask includes the
     * stripped 0x20 (SlowGame) bit plus a real bit. Expect reasons & ~0x20. */
    {
        CSetTargetFramerateMsg msg = CSET_TARGET_FRAMERATE_MSG__INIT;
        msg.framerate = 60;
        msg.has_framerate_numerator = 1;
        msg.framerate_numerator = 60000;
        msg.has_framerate_denominator = 1;
        msg.framerate_denominator = 1001;
        msg.has_reasons = 1;
        msg.reasons = 0x20 | IHS_StreamFramerateSlowNetwork;
        dispatch_video_msg(&channel, k_EStreamControlSetTargetFramerate, (const ProtobufCMessage *) &msg);
        assert(log.set_target_framerate_calls == 1);
        assert(log.framerate_num == 60000);
        assert(log.framerate_denom == 1001);
        assert(log.framerate_reasons == IHS_StreamFramerateSlowNetwork);
    }

    /* SetTargetFramerate — legacy `framerate` only, no fraction; expect
     * fallback to num=framerate, denom=1, reasons=0. */
    {
        CSetTargetFramerateMsg msg = CSET_TARGET_FRAMERATE_MSG__INIT;
        msg.framerate = 30;
        dispatch_video_msg(&channel, k_EStreamControlSetTargetFramerate, (const ProtobufCMessage *) &msg);
        assert(log.set_target_framerate_calls == 2);
        assert(log.framerate_num == 30);
        assert(log.framerate_denom == 1);
        assert(log.framerate_reasons == 0);
    }

    /* SetTargetBitrate — must no longer fall through silently. */
    {
        CSetTargetBitrateMsg msg = CSET_TARGET_BITRATE_MSG__INIT;
        msg.bitrate = 8000000;
        dispatch_video_msg(&channel, k_EStreamControlSetTargetBitrate, (const ProtobufCMessage *) &msg);
        assert(log.set_target_bitrate_calls == 1);
        assert(log.target_bitrate == 8000000);
    }

    /* SetQualityOverride — must no longer abort(). */
    {
        CSetQualityOverrideMsg msg = CSET_QUALITY_OVERRIDE_MSG__INIT;
        msg.has_value = 1;
        msg.value = 70;
        dispatch_video_msg(&channel, k_EStreamControlSetQualityOverride, (const ProtobufCMessage *) &msg);
        assert(log.set_quality_override_calls == 1);
        assert(log.quality_override == 70);
    }

    /* SetBitrateOverride — must no longer abort(). 0 is the "cleared" sentinel. */
    {
        CSetBitrateOverrideMsg msg = CSET_BITRATE_OVERRIDE_MSG__INIT;
        msg.has_value = 1;
        msg.value = 0;
        dispatch_video_msg(&channel, k_EStreamControlSetBitrateOverride, (const ProtobufCMessage *) &msg);
        assert(log.set_bitrate_override_calls == 1);
        assert(log.bitrate_override == 0);
    }

    /* EnableHighResCapture / DisableHighResCapture — empty payload, must
     * decode and log without aborting (no callback fires). */
    {
        CEnableHighResCaptureMsg en = CENABLE_HIGH_RES_CAPTURE_MSG__INIT;
        dispatch_video_msg(&channel, k_EStreamControlEnableHighResCapture, (const ProtobufCMessage *) &en);
        CDisableHighResCaptureMsg dis = CDISABLE_HIGH_RES_CAPTURE_MSG__INIT;
        dispatch_video_msg(&channel, k_EStreamControlDisableHighResCapture, (const ProtobufCMessage *) &dis);
        /* No callback expected; the only assertion is "did not abort". */
    }

    IHS_SessionDestroy(session);
    IHS_Quit();
    return 0;
}
