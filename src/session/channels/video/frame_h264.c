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

#include "frame_h264.h"
#include "ch_data_video.h"

const static uint8_t startSeq[] = {0x00, 0x00, 0x00, 0x01};

static size_t EscapeNAL(uint8_t *out, const uint8_t *src, size_t inLen);

void IHS_SessionVideoFrameAppendH264(IHS_Buffer *buffer, const uint8_t *data, size_t len,
                                     const IHS_VideoFrameHeader *header) {
    if (header->flags & VideoFrameFlagNeedEscape) {
        size_t escapedCap = (len * 3) / 2 + 1;
        if (header->flags & VideoFrameFlagNeedStartSequence) {
            assert(len >= 1);
            IHS_BufferAppendMem(buffer, startSeq, sizeof(startSeq));
        }
        size_t escapedLen = EscapeNAL(IHS_BufferPointerForAppend(buffer, escapedCap), data, len);
        buffer->size += escapedLen;
    } else {
        IHS_BufferAppendMem(buffer, data, len);
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
