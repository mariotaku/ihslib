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

#include "session/channels/ch_data.h"
#include "ch_data_video.h"

#include "crypto.h"
#include "endianness.h"
#include "session/channels/video/callback_h264.h"


#define VIDEO_FRAME_HEADER_SIZE 7

static void ChannelVideoInit(IHS_SessionChannel *channel, const void *config);

static void ChannelVideoDeinit(IHS_SessionChannel *channel);

static bool DataStart(IHS_SessionChannel *channel);

static void DataReceived(IHS_SessionChannel *channel, const IHS_SessionDataFrameHeader *header,
                         const uint8_t *data, size_t len);

static void DataStop(IHS_SessionChannel *channel);

static size_t VideoFrameHeaderParse(IHS_SessionVideoFrameHeader *header, const uint8_t *data);

static void PreprocessAndSubmit(IHS_SessionChannel *channel, const uint8_t *data, size_t len,
                                const IHS_SessionVideoFrameHeader *header);

static const IHS_SessionChannelDataClass ChannelClass = {
        {
                .init = ChannelVideoInit,
                .deinit = ChannelVideoDeinit,
                .received = IHS_SessionChannelDataReceived,
                .instanceSize = sizeof(IHS_SessionChannelVideo)
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
    IHS_SessionChannelVideo *videoCh = (IHS_SessionChannelVideo *) channel;
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
    IHS_SessionChannelVideo *videoCh = (IHS_SessionChannelVideo *) channel;
    if (videoCh->config.codecData) {
        free(videoCh->config.codecData);
    }
}

static bool DataStart(struct IHS_SessionChannel *channel) {
    IHS_SessionChannelVideo *videoCh = (IHS_SessionChannelVideo *) channel;
    const IHS_StreamVideoCallbacks *callbacks = channel->session->videoCallbacks;
    if (!callbacks->start) return true;
    if (callbacks->start(channel->session->videoContext, &videoCh->config) != 0) {
        return false;
    }

    CVideoDecoderInfoMsg message = CVIDEO_DECODER_INFO_MSG__INIT;
    message.info = "Marvell hardware decoding";
    message.has_threads = true;
    message.threads = 1;

    return IHS_SessionSendControlMessage(channel->session, k_EStreamControlVideoDecoderInfo,
                                         (const ProtobufCMessage *) &message, IHS_PACKET_ID_NEXT);
}

static void DataReceived(struct IHS_SessionChannel *channel, const IHS_SessionDataFrameHeader *header,
                         const uint8_t *data, size_t len) {
    IHS_SessionChannelVideo *videoCh = (IHS_SessionChannelVideo *) channel;
    size_t offset = 0;
    IHS_SessionVideoFrameHeader vhead;
    offset += VideoFrameHeaderParse(&vhead, &data[offset]);
    if (vhead.flags & VideoFrameFlagKeyFrame) {
        videoCh->waitingKeyFrame = false;
        videoCh->expectedSequence = vhead.sequence;
    }
    if (vhead.sequence != videoCh->expectedSequence) {
        IHS_SessionLog(channel->session, IHS_BaseLogLevelWarn, "Expected video frame sequence %u, got %u",
                       videoCh->expectedSequence, vhead.sequence);
        IHS_SessionChannelDataLost(channel);
        videoCh->waitingKeyFrame = true;
        videoCh->expectedSequence = vhead.sequence;
    }
    videoCh->expectedSequence++;
    if (videoCh->waitingKeyFrame) return;
    if (vhead.flags & VideoFrameFlagEncrypted) {
        const IHS_SessionConfig *config = &channel->session->config;
        uint8_t *decrypted = malloc(len);
        size_t decryptedLen = len;
        IHS_CryptoSymmetricDecryptWithIV(data, len, EmptyIV, sizeof(EmptyIV),
                                         config->sessionKey, config->sessionKeyLen, decrypted, &decryptedLen);
        PreprocessAndSubmit(channel, decrypted, decryptedLen, &vhead);
        free(decrypted);
    } else {
        PreprocessAndSubmit(channel, &data[offset], len - offset, &vhead);
    }
}

static void DataStop(struct IHS_SessionChannel *channel) {
    const IHS_StreamVideoCallbacks *callbacks = channel->session->videoCallbacks;
    if (!callbacks->stop) return;
    callbacks->stop(channel->session->videoContext);
}

static size_t VideoFrameHeaderParse(IHS_SessionVideoFrameHeader *header, const uint8_t *data) {
    size_t offset = 0;
    offset += IHS_ReadUInt16LE(&data[offset], &header->sequence);
    header->flags = data[offset++];
    offset += IHS_ReadUInt16LE(&data[offset], &header->reserved1);
    offset += IHS_ReadUInt16LE(&data[offset], &header->reserved2);
    return offset;
}


static void PreprocessAndSubmit(IHS_SessionChannel *channel, const uint8_t *data, size_t len,
                                const IHS_SessionVideoFrameHeader *header) {
    IHS_SessionChannelVideo *videoCh = (IHS_SessionChannelVideo *) channel;
    assert (videoCh->config.codec == IHS_StreamVideoCodecH264);
    IHS_SessionVideoFrameSubmitH264(channel, data, len, header);
}
