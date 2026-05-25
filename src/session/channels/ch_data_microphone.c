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
#include <memory.h>

#include "ch_data_microphone.h"
#include "ch_data.h"
#include "session/session_pri.h"
#include "ihs_buffer_ext.h"
#include "ihs_thread.h"

// Mirrors CStreamClient::SendMicrophoneData @ 0x1f805c. Steam's wire format for a mic
// frame: data-channel frame, body = [k_EStreamDataPacket=1] [12-byte frame header]
// [opus/PCM payload]. The frame header carries a monotonic frameId; the other three
// fields (frameTimestamp, inputMark, inputRecvTimestamp) are written as zero — mic
// frames don't participate in the video frame-timing / input-marking subsystem.
typedef struct ChannelMicrophone {
    IHS_SessionChannel base;
    IHS_StreamAudioConfig config;
    IHS_Mutex *sendLock;
    uint16_t nextFrameId;
} ChannelMicrophone;

static void ChannelMicrophoneInit(IHS_SessionChannel *channel, const void *config);

static void ChannelMicrophoneDeinit(IHS_SessionChannel *channel);

static const IHS_SessionChannelClass ChannelClass = {
        .init = ChannelMicrophoneInit,
        .deinit = ChannelMicrophoneDeinit,
        // Mic is send-only. The server may never send anything on this channel; if it
        // does, drop it on the floor — we have no consumer side and Steam's reference
        // client doesn't subscribe to anything here either.
        .received = IHS_SessionChannelReceivedPacketNoop,
        .stopped = NULL,
        .instanceSize = sizeof(ChannelMicrophone),
};

IHS_SessionChannel *IHS_SessionChannelDataMicrophoneCreate(IHS_Session *session, const CStartAudioDataMsg *message) {
    return IHS_SessionChannelCreate(&ChannelClass, session, IHS_SessionChannelTypeDataMicrophone, message->channel,
                                    message);
}

static void ChannelMicrophoneInit(IHS_SessionChannel *channel, const void *config) {
    ChannelMicrophone *mic = (ChannelMicrophone *) channel;
    const CStartAudioDataMsg *message = config;
    mic->config.channels = message->channels;
    mic->config.frequency = message->frequency;
    mic->config.codec = (IHS_StreamAudioCodec) message->codec;
    if (message->has_codec_data) {
        mic->config.codecDataLen = message->codec_data.len;
        mic->config.codecData = malloc(message->codec_data.len);
        memcpy(mic->config.codecData, message->codec_data.data, message->codec_data.len);
    }
    mic->sendLock = IHS_MutexCreate();
    mic->nextFrameId = 0;

    IHS_Session *session = channel->session;
    const IHS_StreamMicrophoneCallbacks *callbacks = session->callbacks.microphone;
    if (callbacks && callbacks->start) {
        // The user is expected to open the platform capture device here. If they refuse
        // (return non-zero), the channel still exists but nothing will ever Send — fine,
        // since the server's Stop will tear it down soon enough.
        callbacks->start(session, &mic->config, session->callbackContexts.microphone);
    }
}

static void ChannelMicrophoneDeinit(IHS_SessionChannel *channel) {
    ChannelMicrophone *mic = (ChannelMicrophone *) channel;
    IHS_Session *session = channel->session;
    const IHS_StreamMicrophoneCallbacks *callbacks = session->callbacks.microphone;
    if (callbacks && callbacks->stop) {
        callbacks->stop(session, session->callbackContexts.microphone);
    }
    if (mic->config.codecData) {
        free(mic->config.codecData);
    }
    IHS_MutexDestroy(mic->sendLock);
}

bool IHS_SessionChannelDataMicrophoneSend(IHS_SessionChannel *channel, const uint8_t *data, size_t len) {
    ChannelMicrophone *mic = (ChannelMicrophone *) channel;
    IHS_SessionFrame frame;
    IHS_SessionChannelInitializeFrame(channel, &frame, IHS_SessionPacketTypeUnreliable, true, IHS_PACKET_ID_NEXT);
    IHS_MutexLock(mic->sendLock);
    uint16_t frameId = mic->nextFrameId++;
    IHS_MutexUnlock(mic->sendLock);

    // Body layout matches what ch_data.c:ReceivedFrame() expects on the inbound side:
    // type byte + 12-byte data-frame header + payload.
    IHS_BufferAppendUInt8(&frame.body, k_EStreamDataPacket);
    IHS_BufferAppendUInt16LE(&frame.body, frameId);
    IHS_BufferAppendUInt32LE(&frame.body, 0); // frameTimestamp — Steam sends 0 for mic
    IHS_BufferAppendUInt16LE(&frame.body, 0); // inputMark
    IHS_BufferAppendUInt32LE(&frame.body, 0); // inputRecvTimestamp
    IHS_BufferAppendMem(&frame.body, data, len);

    // Mic frames are unreliable: voice is time-sensitive and a retransmitted stale
    // frame is worse than a dropped fresh one. Matches Steam's IsReliableData defaulting
    // to false unless the negotiated config explicitly enables reliable data.
    bool ret = IHS_SessionChannelQueueFrame(channel, &frame, false);
    IHS_BufferClear(&frame.body, true);
    return ret;
}
