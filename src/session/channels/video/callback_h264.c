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

static size_t EscapeNAL(uint8_t *out, const uint8_t *src, size_t inLen);

void IHS_SessionVideoFrameSubmitH264(IHS_SessionChannel *channel, const uint8_t *data, size_t len,
                                     const IHS_SessionVideoFrameHeader *header) {
    IHS_Session *session = channel->session;
    const IHS_StreamVideoCallbacks *callbacks = session->callbacks.video;
    if (!callbacks || !callbacks->submit) return;
    void *context = session->callbackContexts.video;
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
            const static uint8_t startSeq[] = {0x00, 0x00, 0x00, 0x01};
            memcpy(&escaped[escapedLen], startSeq, sizeof(startSeq));
            escapedLen += sizeof(startSeq);
        }
        escapedLen += EscapeNAL(&escaped[escapedLen], data, len);
        callbacks->submit(session, escaped, escapedLen, header->sequence, flags, context);
        free(escaped);
    } else {
        callbacks->submit(session, data, len, header->sequence, flags, context);
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

