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

#include "ihs_buffer.h"

#include <stdlib.h>
#include <assert.h>
#include <string.h>

/*
 * Setup functions
 */

void IHS_BufferInit(IHS_Buffer *buffer, size_t initialCapacity, size_t maxCapacity) {
    memset(buffer, 0, sizeof(IHS_Buffer));
    buffer->initialCapacity = initialCapacity;
    buffer->maxCapacity = maxCapacity;
}

/*
 * read functions
 */

uint8_t *IHS_BufferPointer(const IHS_Buffer *buffer) {
    return IHS_BufferPointerAt(buffer, 0);
}

uint8_t *IHS_BufferPointerAt(const IHS_Buffer *buffer, size_t position) {
    assert(buffer->data != NULL);
    assert(position >= 0 && position < IHS_BufferMaxSize(buffer));
    return &buffer->data[buffer->offset + position];
}

void IHS_BufferReadMem(const IHS_Buffer *buffer, size_t position, uint8_t *dest, size_t len) {
    assert(len <= buffer->size);
    memcpy(dest, IHS_BufferPointerAt(buffer, position), len);
}

/*
 * Check functions
 */

void IHS_BufferEnsureCapacityExact(IHS_Buffer *buffer, size_t wantedCapacity) {
    if (buffer->maxCapacity > 0) {
        assert(wantedCapacity <= buffer->maxCapacity);
    }
    if (wantedCapacity <= buffer->capacity) {
        return;
    }
    buffer->data = realloc(buffer->data, wantedCapacity);
    buffer->capacity = wantedCapacity;
    assert(buffer->data != NULL);
}

void IHS_BufferEnsureCapacity(IHS_Buffer *buffer, size_t wantedCapacity) {
    size_t newCapacity = buffer->capacity;
    if (newCapacity == 0) {
        newCapacity = buffer->initialCapacity;
    }
    if (newCapacity == 0) {
        newCapacity = buffer->maxCapacity < 1024 ? buffer->maxCapacity : 1024;
    }
    assert (newCapacity > 0);
    while (newCapacity < wantedCapacity) {
        newCapacity *= 2;
    }
    IHS_BufferEnsureCapacityExact(buffer, newCapacity);
}

void IHS_BufferEnsureMaxSizeExact(IHS_Buffer *buffer, size_t maxSize) {
    IHS_BufferEnsureCapacityExact(buffer, buffer->offset + maxSize + buffer->suffix);
}

void IHS_BufferEnsureMaxSize(IHS_Buffer *buffer, size_t maxSize) {
    IHS_BufferEnsureCapacity(buffer, buffer->offset + maxSize + buffer->suffix);
}

/*
 * write functions: Enough write space will be ensured
 */

void IHS_BufferClear(IHS_Buffer *buffer, bool freeData) {
    buffer->size = 0;
    buffer->offset = 0;
    if (buffer->data == NULL) {
        assert(buffer->capacity == 0);
    } else if (freeData) {
        buffer->capacity = 0;
        free(buffer->data);
        buffer->data = NULL;
    }
}

void IHS_BufferOffsetBy(IHS_Buffer *buffer, int offset) {
    if (offset < 0) {
        assert(-offset <= buffer->offset);
    } else {
        assert(offset <= buffer->size);
    }
    buffer->offset += offset;
    buffer->size -= offset;
}

void IHS_BufferSetSuffixLength(IHS_Buffer *buffer, size_t suffixLen) {
    IHS_BufferEnsureCapacity(buffer, buffer->offset + buffer->size + suffixLen);
    buffer->suffix = suffixLen;
}

void IHS_BufferExtendSize(IHS_Buffer *buffer) {
    buffer->size = buffer->size + buffer->offset + buffer->suffix;
    buffer->offset = 0;
    buffer->suffix = 0;
}

uint8_t *IHS_BufferPointerForAppend(IHS_Buffer *buffer, size_t appendSize) {
    size_t newSize = buffer->size + appendSize;
    IHS_BufferEnsureMaxSize(buffer, newSize);
    return IHS_BufferPointerAt(buffer, buffer->size);
}

uint8_t *IHS_BufferSuffixPointer(IHS_Buffer *buffer) {
    assert(buffer->suffix > 0);
    return IHS_BufferPointerAt(buffer, buffer->size);
}

void IHS_BufferAppend(IHS_Buffer *buffer, const IHS_Buffer *data) {
    IHS_BufferAppendMem(buffer, IHS_BufferPointer(data), data->size);
}

void IHS_BufferAppendMem(IHS_Buffer *buffer, const uint8_t *data, size_t dataLen) {
    uint8_t *dst = IHS_BufferPointerForAppend(buffer, dataLen);
    memcpy(dst, data, dataLen);
    buffer->size += dataLen;
}

void IHS_BufferWriteMem(IHS_Buffer *buffer, size_t position, const uint8_t *src, size_t srcLen) {
    size_t writeEnd = position + srcLen;
    IHS_BufferEnsureMaxSize(buffer, writeEnd);
    uint8_t *dst = IHS_BufferPointerAt(buffer, position);
    memcpy(dst, src, srcLen);
    if (buffer->size < writeEnd) {
        buffer->size = writeEnd;
    }
}

void IHS_BufferFillMem(IHS_Buffer *buffer, size_t position, uint8_t fill, size_t fillLen) {
    size_t fillEnd = position + fillLen;
    IHS_BufferEnsureMaxSize(buffer, fillEnd);
    uint8_t *dst = IHS_BufferPointerAt(buffer, position);
    memset(dst, fill, fillLen);
    if (buffer->size < fillEnd) {
        buffer->size = fillEnd;
    }
}

void IHS_BufferReleaseOwnership(IHS_Buffer *buffer) {
    buffer->data = NULL;
    buffer->capacity = 0;
    IHS_BufferClear(buffer, false);
}

void IHS_BufferTransferOwnership(IHS_Buffer *buffer, IHS_Buffer *to) {
    assert(buffer != to);
#ifndef IHSLIB_SANITIZE_ADDRESS
    *to = *buffer;
#else
    *to = *buffer;
    to->data = malloc(buffer->capacity);
    memcpy(to->data, buffer->data, buffer->capacity);
    free(buffer->data);
#endif
    IHS_BufferReleaseOwnership(buffer);
}