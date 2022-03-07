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

#include "ch_data_video.h"
#include "ch_data.h"

#include "crypto.h"

typedef struct ChannelVideo {
    IHS_SessionChannelData base;
    IHS_StreamVideoConfig config;
} ChannelVideo;

typedef struct VideoFrameHeader {
    uint16_t sequence;
    uint8_t flags;
    uint16_t reserved1;
    uint16_t reserved2;
} VideoFrameHeader;

#define VIDEO_FRAME_HEADER_SIZE 7

enum {
    VideoFrameFlagProtected = 0x10,
    VideoFrameFlagEncrypted = 0x20,
};

static void ChannelVideoInit(IHS_SessionChannel *channel, const void *config);

static void ChannelVideoDeinit(IHS_SessionChannel *channel);

static void DataStart(IHS_SessionChannel *channel);

static void DataReceived(IHS_SessionChannel *channel, const IHS_SessionDataFrameHeader *header,
                         const uint8_t *data, size_t len);

static void DataStop(IHS_SessionChannel *channel);

static size_t VideoFrameHeaderParse(VideoFrameHeader *header, const uint8_t *data);

static const IHS_SessionChannelDataClass ChannelClass = {
        {
                .init = ChannelVideoInit,
                .deinit = ChannelVideoDeinit,
                .received = IHS_SessionChannelDataReceived,
                .instanceSize = sizeof(ChannelVideo)
        },
        .start = DataStart,
        .dataFrame = DataReceived,
        .stop = DataStop,
};

static const uint8_t EmptyIV[16] = {
        0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0,
};

IHS_SessionChannel *IHS_SessionChannelDataVideoCreate(IHS_Session *session, const CStartVideoDataMsg *message) {
    return IHS_SessionChannelDataCreate(&ChannelClass, session, IHS_SessionChannelTypeDataVideo,
                                        message->channel, (void *) message);
}

static void ChannelVideoInit(IHS_SessionChannel *channel, const void *config) {
    ChannelVideo *videoCh = (ChannelVideo *) channel;
    const CStartVideoDataMsg *message = config;
    videoCh->config.width = message->width;
    videoCh->config.height = message->height;
    videoCh->config.codec = (IHS_StreamVideoCodec) message->codec;
    if (message->has_codec_data) {
        videoCh->config.codecDataLen = message->codec_data.len;
        videoCh->config.codecData = malloc(message->codec_data.len);
        memcpy(videoCh->config.codecData, message->codec_data.data, message->codec_data.len);
    }
    IHS_SessionChannelDataInit(channel);
}

static void ChannelVideoDeinit(IHS_SessionChannel *channel) {
    IHS_SessionChannelDataDeinit(channel);
    ChannelVideo *videoCh = (ChannelVideo *) channel;
    if (videoCh->config.codecData) {
        free(videoCh->config.codecData);
    }
}

static void DataStart(struct IHS_SessionChannel *channel) {
    ChannelVideo *videoCh = (ChannelVideo *) channel;
    const IHS_StreamVideoCallbacks *callbacks = channel->session->videoCallbacks;
    if (!callbacks->start) return;
    callbacks->start(channel->session->videoContext, &videoCh->config);
}

static void DataReceived(struct IHS_SessionChannel *channel, const IHS_SessionDataFrameHeader *header,
                         const uint8_t *data, size_t len) {
    const IHS_StreamVideoCallbacks *callbacks = channel->session->videoCallbacks;
    if (!callbacks->received) return;
    size_t offset = 0;
    VideoFrameHeader vhead;
    offset += VideoFrameHeaderParse(&vhead, &data[offset]);
    if (vhead.flags & VideoFrameFlagEncrypted) {
        const IHS_SessionConfig *config = &channel->session->config;
        uint8_t *decrypted = malloc(len - offset);
        size_t decryptedLen = len - offset;
        IHS_CryptoSymmetricDecryptWithIV(&data[offset], len - offset, EmptyIV, sizeof(EmptyIV),
                                         config->sessionKey, config->sessionKeyLen, decrypted, &decryptedLen);
        callbacks->received(channel->session->videoContext, decrypted, decryptedLen, vhead.sequence);
        free(decrypted);
    } else {
        callbacks->received(channel->session->videoContext, data, len, vhead.sequence);
    }
}

static void DataStop(struct IHS_SessionChannel *channel) {
    const IHS_StreamVideoCallbacks *callbacks = channel->session->videoCallbacks;
    if (!callbacks->stop) return;
    callbacks->stop(channel->session->videoContext);
}

static size_t VideoFrameHeaderParse(VideoFrameHeader *header, const uint8_t *data) {
    return VIDEO_FRAME_HEADER_SIZE;
}