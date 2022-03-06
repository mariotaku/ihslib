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

#include <malloc.h>
#include <memory.h>

#include "ch_data_audio.h"
#include "ch_data.h"

typedef struct ChannelAudio {
    IHS_SessionChannelData base;
    IHS_StreamAudioConfig config;
} ChannelAudio;

static void ChannelAudioInit(IHS_SessionChannel *channel, const void *config);

static void ChannelAudioDeinit(IHS_SessionChannel *channel);

static void DataStart(struct IHS_SessionChannel *channel);

static void DataReceived(struct IHS_SessionChannel *channel, const IHS_SessionFrame *frame);

static void DataStop(struct IHS_SessionChannel *channel);

static const IHS_SessionChannelDataClass ChannelClass = {
        {
                .init = ChannelAudioInit,
                .deinit = ChannelAudioDeinit,
                .received = IHS_SessionChannelDataReceived,
                .instanceSize = sizeof(ChannelAudio)
        },
        .start = DataStart,
        .received = DataReceived,
        .stop = DataStop,
};

IHS_SessionChannel *IHS_SessionChannelDataAudioCreate(IHS_Session *session, const CStartAudioDataMsg *message) {
    return IHS_SessionChannelDataCreate(&ChannelClass, session, IHS_SessionChannelTypeDataAudio, message->channel,
                                        message);
}

static void ChannelAudioInit(IHS_SessionChannel *channel, const void *config) {
    ChannelAudio *audioCh = (ChannelAudio *) channel;
    const CStartAudioDataMsg *message = config;
    audioCh->config.channels = message->channels;
    audioCh->config.frequency = message->frequency;
    audioCh->config.codec = (IHS_StreamAudioCodec) message->codec;
    if (message->has_codec_data) {
        audioCh->config.codecDataLen = message->codec_data.len;
        audioCh->config.codecData = malloc(message->codec_data.len);
        memcpy(audioCh->config.codecData, message->codec_data.data, message->codec_data.len);
    }
    IHS_SessionChannelDataInit(channel);
}

static void ChannelAudioDeinit(IHS_SessionChannel *channel) {
    IHS_SessionChannelDataDeinit(channel);
    ChannelAudio *audioCh = (ChannelAudio *) channel;
    if (audioCh->config.codecData) {
        free(audioCh->config.codecData);
    }
}

static void DataStart(struct IHS_SessionChannel *channel) {
    ChannelAudio *audioCh = (ChannelAudio *) channel;
    const IHS_StreamAudioCallbacks *callbacks = channel->session->audioCallbacks;
    if (!callbacks->start) return;
    callbacks->start(channel->session->audioContext, &audioCh->config);
}

static void DataReceived(struct IHS_SessionChannel *channel, const IHS_SessionFrame *frame) {
    const IHS_StreamAudioCallbacks *callbacks = channel->session->audioCallbacks;
    if (!callbacks->received) return;
    callbacks->received(channel->session->audioContext, frame->body, frame->bodyLen);
}

static void DataStop(struct IHS_SessionChannel *channel) {
    const IHS_StreamAudioCallbacks *callbacks = channel->session->audioCallbacks;
    if (!callbacks->stop) return;
    callbacks->stop(channel->session->audioContext);
}