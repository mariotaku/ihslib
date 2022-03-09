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

#include "ch_data.h"
#include "ch_data_video.h"
#include "video/partial_frame.h"

#include "crypto.h"
#include "endianness.h"

typedef struct ChannelVideo {
    IHS_SessionChannelData base;
    IHS_StreamVideoConfig config;
    uint16_t expectedSequence;
    bool waitingKeyFrame;
    IHS_SessionVideoPartialFrame *partialFrames;
} ChannelVideo;

#define VIDEO_FRAME_HEADER_SIZE 7

static void ChannelVideoInit(IHS_SessionChannel *channel, const void *config);

static void ChannelVideoDeinit(IHS_SessionChannel *channel);

static void DataStart(IHS_SessionChannel *channel);

static void DataReceived(IHS_SessionChannel *channel, const IHS_SessionDataFrameHeader *header,
                         const uint8_t *data, size_t len);

static void DataStop(IHS_SessionChannel *channel);

static size_t VideoFrameHeaderParse(IHS_SessionVideoFrameHeader *header, const uint8_t *data);

static void NotifyReceived(IHS_SessionChannel *channel, const uint8_t *data, size_t len, uint8_t flags);

static void ConsumePartialFrames(IHS_SessionChannel *channel);

static size_t EscapeNAL(uint8_t *out, const uint8_t *src, size_t inLen);


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
    IHS_VideoPartialFrameClear(videoCh->partialFrames);
    if (videoCh->config.codecData) {
        free(videoCh->config.codecData);
    }
}

static void DataStart(struct IHS_SessionChannel *channel) {
    ChannelVideo *videoCh = (ChannelVideo *) channel;
    const IHS_StreamVideoCallbacks *callbacks = channel->session->videoCallbacks;
    if (!callbacks->start) return;
    callbacks->start(channel->session->videoContext, &videoCh->config);

    CVideoDecoderInfoMsg message = CVIDEO_DECODER_INFO_MSG__INIT;
    message.info = "Marvell hardware decoding";
    message.has_threads = true;
    message.threads = 1;

    IHS_SessionSendControlMessage(channel->session, k_EStreamControlVideoDecoderInfo,
                                  (const ProtobufCMessage *) &message, IHS_PACKET_ID_NEXT);
}

static void DataReceived(struct IHS_SessionChannel *channel, const IHS_SessionDataFrameHeader *header,
                         const uint8_t *data, size_t len) {
    ChannelVideo *videoCh = (ChannelVideo *) channel;
    size_t offset = 0;
    IHS_SessionVideoFrameHeader vhead;
    offset += VideoFrameHeaderParse(&vhead, &data[offset]);
    if (vhead.flags & VideoFrameFlagKeyFrame) {
        videoCh->waitingKeyFrame = false;
        videoCh->expectedSequence = vhead.sequence;
    }
    if (vhead.sequence != videoCh->expectedSequence) {
        fprintf(stderr, "Expected sequence %u, got %u\n", videoCh->expectedSequence,
                vhead.sequence);
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
//        videoCh->partialFrames = IHS_VideoPartialFrameInsert(videoCh->partialFrames, &vhead,
//                                                             decrypted, decryptedLen);
        NotifyReceived(channel, decrypted, decryptedLen, vhead.flags);
        free(decrypted);
    } else {
//        videoCh->partialFrames = IHS_VideoPartialFrameInsert(videoCh->partialFrames, &vhead,
//
//                                                             &data[offset], len - offset);
        NotifyReceived(channel, &data[offset], len - offset, vhead.flags);
    }
//    ConsumePartialFrames(channel);
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


static void ConsumePartialFrames(IHS_SessionChannel *channel) {
    ChannelVideo *videoCh = (ChannelVideo *) channel;

    IHS_SessionVideoPartialFrame *finishedNode = NULL;
    uint16_t partialReserved1 = 0;
    size_t frameLen = 0;
    IHS_VideoPartialFrameForEach(videoCh->partialFrames, cur) {
//        if (cur->reserved2) {
//            printf("cur->reserved2=%u\n", cur->reserved2);
//            if (cur->reserved1 != partialReserved1) {
//                if (finishedNode) {
//                    printf("Decode this frame\n");
//                    break;
//                }
//            }
//            if (cur->flags & VideoFrameFlagReserved1Increment) {
//                if (cur->flags & VideoFrameFlagFrameFinish) {
//                    partialReserved1 = 0;
//                } else {
//                    partialReserved1 = cur->reserved2 + 1;
//                }
//            }
//        }
        frameLen += cur->dataLen;
        // TODO append buffer
        if (cur->flags & VideoFrameFlagFrameFinish) {
            finishedNode = cur;
        }
    }
    if (!finishedNode || !frameLen) return;

    IHS_SessionVideoPartialFrame *oldHead = videoCh->partialFrames;
    IHS_SessionVideoPartialFrame *newHead = finishedNode->next;
    if (newHead) {
        newHead->prev = NULL;
    }
    finishedNode->next = NULL;
    videoCh->partialFrames = newHead;

    uint8_t *frameData = malloc(frameLen);
    size_t frameSize = 0;
    IHS_VideoPartialFrameForEach(oldHead, cur) {
        memcpy(&frameData[frameSize], cur->data, cur->dataLen);
        frameSize += cur->dataLen;
    }
    NotifyReceived(channel, frameData, frameSize, oldHead->flags);
    free(frameData);
    IHS_VideoPartialFrameClear(oldHead);
}

static void NotifyReceived(IHS_SessionChannel *channel, const uint8_t *data, size_t len, uint8_t hflags) {
    const IHS_StreamVideoCallbacks *callbacks = channel->session->videoCallbacks;
    if (!callbacks->received) return;
    void *context = channel->session->videoContext;
    uint8_t flags = IHS_StreamVideoFrameNone;
    if (hflags & VideoFrameFlagKeyFrame) {
        flags |= IHS_StreamVideoFrameKeyFrame;
    }
    if (hflags & VideoFrameFlagNeedEscape) {
        size_t escapedCap = (len * 3) / 2 + 5;
        uint8_t *escaped = malloc(escapedCap);
        size_t escapedLen = 0;
        if (hflags & VideoFrameFlagNeedStartSequence) {
            assert(len >= 1);
            const static uint8_t startSeq[] = {0x00, 0x00, 0x00, 0x01};
            memcpy(&escaped[escapedLen], startSeq, sizeof(startSeq));
            escapedLen += sizeof(startSeq);
        }
        escapedLen += EscapeNAL(&escaped[escapedLen], data, len);
        callbacks->received(context, escaped, escapedLen, 0, flags);
        free(escaped);
    } else {
        callbacks->received(context, data, len, 0, flags);
    }
}

static size_t EscapeNAL(uint8_t *out, const uint8_t *src, size_t inLen) {
    uint8_t *dst = out;
    const uint8_t *end = src + inLen;
    if (src < end) *dst++ = *src++;
    if (src < end) *dst++ = *src++;
    while (src < end) {
        if (src[0] <= 0x03 && !dst[-2] && !dst[-1])
            *dst++ = 0x03;
        *dst++ = *src++;
    }
    return dst - out;
}
