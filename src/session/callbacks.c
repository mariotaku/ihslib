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
#include "session_pri.h"
#include "frame_stats.h"
#include "packet.h"
#include "channels/ch_data_microphone.h"

void IHS_SessionSetSessionCallbacks(IHS_Session *session, const IHS_StreamSessionCallbacks *callbacks, void *context) {
    IHS_BaseLock(&session->base);
    session->callbacks.session = callbacks;
    session->callbackContexts.session = context;
    IHS_BaseUnlock(&session->base);
}

void IHS_SessionSetAudioCallbacks(IHS_Session *session, const IHS_StreamAudioCallbacks *callbacks, void *context) {
    IHS_BaseLock(&session->base);
    session->callbacks.audio = callbacks;
    session->callbackContexts.audio = context;
    IHS_BaseUnlock(&session->base);
}

void IHS_SessionSetVideoCallbacks(IHS_Session *session, const IHS_StreamVideoCallbacks *callbacks, void *context) {
    IHS_BaseLock(&session->base);
    session->callbacks.video = callbacks;
    session->callbackContexts.video = context;
    IHS_BaseUnlock(&session->base);
}

void IHS_SessionSetInputCallbacks(IHS_Session *session, const IHS_StreamInputCallbacks *callbacks, void *context) {
    IHS_BaseLock(&session->base);
    session->callbacks.input = callbacks;
    session->callbackContexts.input = context;
    IHS_BaseUnlock(&session->base);
}

void IHS_SessionSetMicrophoneCallbacks(IHS_Session *session, const IHS_StreamMicrophoneCallbacks *callbacks,
                                       void *context) {
    IHS_BaseLock(&session->base);
    session->callbacks.microphone = callbacks;
    session->callbackContexts.microphone = context;
    IHS_BaseUnlock(&session->base);
}

bool IHS_SessionSendMicrophoneData(IHS_Session *session, const uint8_t *data, size_t len) {
    // No channel = server hasn't started mic (or already stopped). Caller should drop.
    // Looking it up each call keeps the function safe to invoke from any thread without
    // the caller having to track channel lifetime.
    IHS_SessionChannel *mic = IHS_SessionChannelForType(session, IHS_SessionChannelTypeDataMicrophone);
    if (mic == NULL) return false;
    return IHS_SessionChannelDataMicrophoneSend(mic, data, len);
}

void IHS_SessionSetLogFunction(IHS_Session *session, IHS_LogFunction *logFunction) {
    IHS_BaseSetLogFunction(&session->base, logFunction);
}

void IHS_SessionReportVideoFrameStage(IHS_Session *session, uint16_t frameId,
                                      IHS_VideoFrameStage stage, uint32_t timestamp) {
    if (session->frameStats == NULL) {
        return;
    }
    if (timestamp == 0) {
        timestamp = IHS_SessionPacketTimestamp();
    }
    IHS_FrameStatsRecordStage(session->frameStats, frameId, stage, timestamp);
}

void IHS_SessionReportVideoFrameComplete(IHS_Session *session, uint16_t frameId,
                                         IHS_VideoFrameResult result) {
    if (session->frameStats == NULL) {
        return;
    }
    /* Stamp event 18 (Complete) ourselves before delegating, so the aggregator's
     * fold step can compute Client end-to-end and inter-frame interval. */
    IHS_FrameStatsRecordStage(session->frameStats, frameId,
                              (IHS_VideoFrameStage) 18 /* k_EStreamFrameEventComplete */,
                              IHS_SessionPacketTimestamp());
    IHS_FrameStatsRecordComplete(session->frameStats, frameId, result);
}

void IHS_SessionStatsSetFullReporting(IHS_Session *session, bool enabled) {
    if (session->frameStats == NULL) {
        return;
    }
    IHS_FrameStatsAggregatorSetFullReporting(session->frameStats, enabled);
}
