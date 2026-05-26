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

#include <stdlib.h>
#include <stdio.h>

#include "ch_control.h"

#include "video/ch_data_video.h"
#include "session/session_pri.h"

void IHS_SessionChannelControlOnVideo(IHS_SessionChannel *channel, EStreamControlMessage type,
                                      IHS_Buffer *payload, const IHS_SessionPacketHeader *header) {
    IHS_UNUSED(header);
    IHS_Session *session = channel->session;
    switch (type) {
        case k_EStreamControlStartVideoData: {
            IHS_SessionChannel *video = IHS_SessionChannelForType(session, IHS_SessionChannelTypeDataVideo);
            if (video) break;
            CStartVideoDataMsg *message = cstart_video_data_msg__unpack(NULL, payload->size,
                                                                        IHS_BufferPointer(payload));
            if (message == NULL) {
                IHS_SessionLog(session, IHS_LogLevelWarn, "Video", "Malformed CStartVideoDataMsg");
                break;
            }
            video = IHS_SessionChannelDataVideoCreate(session, message);
            IHS_SessionChannelAdd(session, video);
            cstart_video_data_msg__free_unpacked(message, NULL);
            break;
        }
        case k_EStreamControlStopVideoData: {
            IHS_SessionChannel *video = IHS_SessionChannelForType(session, IHS_SessionChannelTypeDataVideo);
            if (!video) break;
            IHS_SessionChannelRemove(session, video->id);
            break;
        }
        case k_EStreamControlVideoEncoderInfo: {
            CVideoEncoderInfoMsg *message = cvideo_encoder_info_msg__unpack(NULL, payload->size,
                                                                            IHS_BufferPointer(payload));
            if (message == NULL) {
                IHS_SessionLog(session, IHS_LogLevelWarn, "Video", "Malformed CVideoEncoderInfoMsg");
                break;
            }
            IHS_SessionLog(session, IHS_LogLevelDebug, "Video", "VideoEncoderInfo(%s)", message->info);
            cvideo_encoder_info_msg__free_unpacked(message, NULL);
            break;
        }
        case k_EStreamControlSetCaptureSize: {
            CSetCaptureSizeMsg *message = cset_capture_size_msg__unpack(NULL, payload->size,
                                                                        IHS_BufferPointer(payload));
            if (message == NULL) {
                IHS_SessionLog(session, IHS_LogLevelWarn, "Video", "Malformed CSetCaptureSizeMsg");
                break;
            }
            IHS_SessionLog(session, IHS_LogLevelDebug, "Video", "SetCaptureSize(width=%d, height=%d)",
                           message->width, message->height);
            const IHS_StreamVideoCallbacks *callbacks = session->callbacks.video;
            if (callbacks && callbacks->setCaptureSize) {
                callbacks->setCaptureSize(session, message->width, message->height, session->callbackContexts.video);
            }
            cset_capture_size_msg__free_unpacked(message, NULL);
            break;
        }
        case k_EStreamControlSetTargetFramerate: {
            CSetTargetFramerateMsg *message = cset_target_framerate_msg__unpack(NULL, payload->size,
                                                                                IHS_BufferPointer(payload));
            if (message == NULL) {
                IHS_SessionLog(session, IHS_LogLevelWarn, "Video", "Malformed CSetTargetFramerateMsg");
                break;
            }
            /* Steam's CStreamClient::OnSetTargetFramerate (0x001fe270) reads the fraction
             * and `reasons`, never the legacy `framerate` uint. Match that — if the host
             * only sent the legacy field, synthesise num/1 so callbacks see a consistent
             * shape. `reasons` is masked with ~0x20 (SlowGame) before reporting. */
            uint32_t num, denom;
            if (message->has_framerate_numerator && message->has_framerate_denominator
                    && message->framerate_denominator != 0) {
                num = message->framerate_numerator;
                denom = message->framerate_denominator;
            } else {
                num = message->framerate;
                denom = 1;
            }
            uint32_t reasons = message->has_reasons ? (message->reasons & ~0x20u) : 0;
            IHS_SessionLog(session, IHS_LogLevelDebug, "Video",
                           "SetTargetFramerate(fps=%.02f, reasons=0x%x)",
                           denom ? (float) num / (float) denom : 0.0f, reasons);
            const IHS_StreamVideoCallbacks *callbacks = session->callbacks.video;
            if (callbacks && callbacks->setTargetFramerate) {
                callbacks->setTargetFramerate(session, num, denom, reasons,
                                              session->callbackContexts.video);
            }
            cset_target_framerate_msg__free_unpacked(message, NULL);
            break;
        }
        case k_EStreamControlSetTargetBitrate: {
            CSetTargetBitrateMsg *message = cset_target_bitrate_msg__unpack(NULL, payload->size,
                                                                            IHS_BufferPointer(payload));
            if (message == NULL) {
                IHS_SessionLog(session, IHS_LogLevelWarn, "Video", "Malformed CSetTargetBitrateMsg");
                break;
            }
            IHS_SessionLog(session, IHS_LogLevelDebug, "Video", "SetTargetBitrate(bps=%d)", message->bitrate);
            const IHS_StreamVideoCallbacks *callbacks = session->callbacks.video;
            if (callbacks && callbacks->setTargetBitrate) {
                callbacks->setTargetBitrate(session, message->bitrate, session->callbackContexts.video);
            }
            cset_target_bitrate_msg__free_unpacked(message, NULL);
            break;
        }
        case k_EStreamControlSetQualityOverride: {
            CSetQualityOverrideMsg *message = cset_quality_override_msg__unpack(NULL, payload->size,
                                                                                IHS_BufferPointer(payload));
            if (message == NULL) {
                IHS_SessionLog(session, IHS_LogLevelWarn, "Video", "Malformed CSetQualityOverrideMsg");
                break;
            }
            int32_t value = message->has_value ? message->value : 0;
            IHS_SessionLog(session, IHS_LogLevelDebug, "Video", "SetQualityOverride(value=%d)", value);
            const IHS_StreamVideoCallbacks *callbacks = session->callbacks.video;
            if (callbacks && callbacks->setQualityOverride) {
                callbacks->setQualityOverride(session, value, session->callbackContexts.video);
            }
            cset_quality_override_msg__free_unpacked(message, NULL);
            break;
        }
        case k_EStreamControlSetBitrateOverride: {
            CSetBitrateOverrideMsg *message = cset_bitrate_override_msg__unpack(NULL, payload->size,
                                                                                IHS_BufferPointer(payload));
            if (message == NULL) {
                IHS_SessionLog(session, IHS_LogLevelWarn, "Video", "Malformed CSetBitrateOverrideMsg");
                break;
            }
            int32_t value = message->has_value ? message->value : 0;
            IHS_SessionLog(session, IHS_LogLevelDebug, "Video", "SetBitrateOverride(value=%d)", value);
            const IHS_StreamVideoCallbacks *callbacks = session->callbacks.video;
            if (callbacks && callbacks->setBitrateOverride) {
                callbacks->setBitrateOverride(session, value, session->callbackContexts.video);
            }
            cset_bitrate_override_msg__free_unpacked(message, NULL);
            break;
        }
        case k_EStreamControlEnableHighResCapture:
        case k_EStreamControlDisableHighResCapture: {
            /* Empty-payload tokens used by Steam's own client→server pinch-zoom
             * path (CStreamPlayer::UpdateHighResCapture). Steam's dispatcher has
             * empty `break;` arms for these on receive — no spec-conformant host
             * sends them. We accept and ignore so a non-conformant peer can't
             * crash the session via the default abort. */
            IHS_SessionLog(session, IHS_LogLevelDebug, "Video", "HighResCapture(%s) — informational, ignored",
                           type == k_EStreamControlEnableHighResCapture ? "enable" : "disable");
            break;
        }
        default: {
            abort();
        }
    }
}
