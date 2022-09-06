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

#include "callback_h264.h"
#include "ch_data_video.h"

#include "memmem.h"
#include "session/session_pri.h"

const static uint8_t startSeq[] = {0x00, 0x00, 0x00, 0x01};

static size_t EscapeNAL(uint8_t *out, const uint8_t *src, size_t inLen);

static const uint8_t *NextUnit(const uint8_t *data, size_t rem);

static void SubmitSliced(IHS_Session *session, const uint8_t *data, size_t len, uint16_t sequence,
                         IHS_StreamVideoFrameFlag flags);

void IHS_SessionVideoFrameSubmitH264(IHS_SessionChannel *channel, const uint8_t *data, size_t len,
                                     const IHS_SessionVideoFrameHeader *header) {
    IHS_Session *session = channel->session;
    const IHS_StreamVideoCallbacks *callbacks = session->callbacks.video;
    if (!callbacks || !callbacks->submit) return;
    IHS_StreamVideoFrameFlag flags = IHS_StreamVideoFrameNone;
    if (header->flags & VideoFrameFlagKeyFrame) {
        flags |= IHS_StreamVideoFrameKeyFrame;
    }
    if (header->flags & VideoFrameFlagNeedEscape) {
        size_t escapedCap = (len * 3) / 2 + 5;
        uint8_t *escaped = malloc(escapedCap);
        size_t escapedLen = 0;
        if (header->flags & VideoFrameFlagNeedStartSequence) {
            assert(len >= 1);
            memcpy(&escaped[escapedLen], startSeq, sizeof(startSeq));
            escapedLen += sizeof(startSeq);
        }
        escapedLen += EscapeNAL(&escaped[escapedLen], data, len);
        SubmitSliced(session, escaped, escapedLen, header->sequence, flags);
        free(escaped);
    } else {
        SubmitSliced(session, data, len, header->sequence, flags);
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

static const uint8_t *NextUnit(const uint8_t *data, size_t rem) {
    if (rem <= 4) {
        return NULL;
    }
    return memmem(data + 4, rem - 4, startSeq, 4);
}

static void SubmitSliced(IHS_Session *session, const uint8_t *data, size_t len, uint16_t sequence,
                         IHS_StreamVideoFrameFlag flags) {
    const IHS_StreamVideoCallbacks *callbacks = session->callbacks.video;
    void *context = session->callbackContexts.video;
    if (!callbacks->config.sliced) {
        callbacks->submit(session, data, len, sequence, 0, flags, context);
        return;
    }

    const uint8_t *cur = data, *next = NULL;
    uint16_t slice = 0;
    size_t rem = len;
    while ((next = NextUnit(cur, rem)) != NULL) {
        size_t slen = next - cur;
        callbacks->submit(session, cur, slen, sequence, slice, flags, context);
        cur = next;
        slice += 1;
        rem -= slen;
    }
    callbacks->submit(session, cur, rem, sequence, slice, flags, context);
}