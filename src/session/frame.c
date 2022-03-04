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

#include "frame.h"
#include "endianness.h"

#include <mbedtls/md.h>
#include <memory.h>
#include <malloc.h>
#include "crypto.h"
#include "session_pri.h"

/**
 *
 * @param window
 */
void IHS_SessionPacketsWindowInit(IHS_SessionPacketsWindow *window, uint16_t capacity) {
    memset(window, 0, sizeof(IHS_SessionPacketsWindow));
    window->capacity = capacity;
    window->data = calloc(capacity, sizeof(IHS_SessionPacketsWindow));
    window->head = 0;
    window->tail = -1;
}

bool IHS_SessionPacketsWindowAdd(IHS_SessionPacketsWindow *window, const IHS_SessionPacket *packet) {
    uint16_t packetId = packet->header.packetId;
    IHS_SessionFramePacket *data = window->data;

    /* Calculate distance of 2 packets */
    int tailOffset = window->tail < 0 ? 1 : (packetId - data[window->tail].header.packetId);
    /* This is not a missing packet before tail, treat it as a rollover */
    if (-tailOffset >= (int) IHS_SessionPacketsWindowSize(window)) {
        tailOffset = UINT16_MAX + tailOffset;
    }
    /* Large offset means too many lost packets */
    if (tailOffset > IHS_SessionPacketsWindowAvailable(window)) {
        return false;
    }
    size_t currTail = window->tail + tailOffset;
    IHS_SessionFramePacket *fp = &data[currTail % window->capacity];
    /* Ignore if the slot is used */
    if (fp->used) {
        return true;
    }
    fp->used = true;
    fp->header = packet->header;
    fp->bodyLen = packet->bodyLen;
    fp->body = malloc(packet->bodyLen);
    memcpy(fp->body, packet->body, packet->bodyLen);

    /* Wrap back to start of the array */
    if (tailOffset > 0) {
        window->tail = (int) (currTail % window->capacity);
    }
    return true;
}

size_t IHS_SessionPacketsWindowAvailable(const IHS_SessionPacketsWindow *window) {
    return window->capacity - IHS_SessionPacketsWindowSize(window);
}

size_t IHS_SessionPacketsWindowSize(const IHS_SessionPacketsWindow *window) {
    if (window->tail < 0) {
        return 0;
    } else if (window->tail >= window->head) {
        /*
         * Head is before tail, just calculate their distance
         *
         * |[+][+][+][+][+][-][-][-]| capacity = 8
         *  ^ head = 0    ^ tail = 4
         * distance = tail - head + 1 = 5
         */
        return window->tail - window->head + 1;
    } else {
        /* Head is after tail
         *
         * |[+][+][-][-][-][-][+][+]| capacity = 8
         *       ^ tail = 2  ^ head = 5
         * distance = capacity - head - 1 + tail = 4
         */
        return window->capacity - window->head - 1 + window->tail;
    }
}

bool IHS_SessionPacketsWindowPoll(IHS_SessionPacketsWindow *window, IHS_SessionFrame *frame) {
    size_t size = IHS_SessionPacketsWindowSize(window);
    if (!size) return false;
    IHS_SessionFramePacket *data = window->data;
    IHS_SessionFramePacket *packet = &data[window->head];
    IHS_SessionPacketType type = packet->header.type;
    /* Must start from packet head */
    if (type != IHS_SessionPacketTypeReliable && type != IHS_SessionPacketTypeUnreliable) {
        return false;
    }
    /* Must have size enough for all fragments */
    int packetsCount = 1 + packet->header.fragmentId;
    if (size < packetsCount) return false;
    size_t frameBodyLen = 0;
    for (int i = window->head, j = window->head + packetsCount; i < j; i++) {
        /* The array is sparse, must collect all fragments */
        const IHS_SessionFramePacket *item = &data[i % window->capacity];
        if (!item->used) return false;
        frameBodyLen += item->bodyLen;
    }
    uint8_t *frameBody = malloc(frameBodyLen);

    frame->header = data[window->head].header;
    frame->body = frameBody;
    frame->bodyLen = frameBodyLen;

    size_t frameBodyOffset = 0;
    for (int i = window->head, j = window->head + packetsCount; i < j; i++) {
        IHS_SessionFramePacket *item = &data[i % window->capacity];
        memcpy(&frameBody[frameBodyOffset], item->body, item->bodyLen);
        frameBodyOffset += item->bodyLen;

        /* This packet is used, recycle it */
        item->used = false;
        free(item->body);
        item->body = NULL;
        item->bodyLen = 0;
    }

    window->head = (window->head + packetsCount) % window->capacity;
    return true;
}

void IHS_SessionPacketsWindowReleaseFrame(IHS_SessionFrame *frame) {
    free(frame->body);
}

int IHS_SessionFrameEncrypt(IHS_Session *session, const uint8_t *in, size_t inLen, uint8_t *out, size_t *outLen,
                            uint64_t sequence) {
    int ret;

    size_t plainLen = sizeof(uint64_t) + inLen;
    uint8_t *plain = malloc(plainLen);
    IHS_WriteUInt64LE(plain, sequence);
    memcpy(&plain[sizeof(uint64_t)], in, inLen);

    const mbedtls_md_info_t *md = mbedtls_md_info_from_type(MBEDTLS_MD_MD5);
    uint8_t *iv = out;
    unsigned char ivLen = mbedtls_md_get_size(md);

    const uint8_t *key = session->state.config.sessionKey;
    const size_t keyLen = session->state.config.sessionKeyLen;

    if ((ret = mbedtls_md_hmac(md, key, keyLen, plain, plainLen, iv)) != 0) {
        goto exit;
    }

    size_t encLen = *outLen - ivLen;
    if ((ret = IHS_CryptoSymmetricEncryptWithIV(plain, plainLen, iv, ivLen, key, keyLen, false,
                                                &out[ivLen], &encLen)) != 0) {
        goto exit;
    }
    *outLen = ivLen + encLen;
    exit:
    free(plain);
    return ret;
}

int IHS_SessionFrameDecrypt(IHS_Session *session, const uint8_t *in, size_t inLen, uint8_t *out, size_t *outLen,
                            uint64_t expectedSequence) {
    const uint8_t *iv = in;

    const uint8_t *key = session->state.config.sessionKey;
    const size_t keyLen = session->state.config.sessionKeyLen;
    int ret;
    if ((ret = IHS_CryptoSymmetricDecryptWithIV(&in[16], inLen - 16, iv, 16, key, keyLen, out, outLen)) != 0) {
        goto exit;
    }

    const mbedtls_md_info_t *md = mbedtls_md_info_from_type(MBEDTLS_MD_MD5);
    uint8_t hash[16];
    if ((ret = mbedtls_md_hmac(md, key, keyLen, out, *outLen, hash)) != 0) {
        goto exit;
    }
    if (memcmp(hash, iv, 16) != 0) {
        ret = -1;
        goto exit;
    }

    *outLen -= 8;
    uint64_t sequence;
    IHS_ReadUInt64LE(out, &sequence);
    memmove(out, &out[8], *outLen);
    if (sequence != expectedSequence) {
        ret = -1;
        goto exit;
    }
    exit:
    return ret;
}

int IHS_SessionFrameHMACSHA256(IHS_Session *session, const uint8_t *in, size_t inLen, uint8_t *out, size_t *outLen) {
    const mbedtls_md_info_t *md = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
    unsigned char mdSize = mbedtls_md_get_size(md);
    if (*outLen < mdSize) {
        return MBEDTLS_ERR_MD_BAD_INPUT_DATA;
    }
    const uint8_t *key = session->state.config.sessionKey;
    const size_t keyLen = session->state.config.sessionKeyLen;
    int ret;
    if ((ret = mbedtls_md_hmac(md, key, keyLen, in, inLen, out)) != 0) {
        return ret;
    }
    *outLen = mdSize;
    return ret;
}