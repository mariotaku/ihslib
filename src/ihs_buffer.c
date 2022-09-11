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

void IHS_BufferInit(IHS_Buffer *buffer, size_t initialCapacity, size_t maxCapacity) {
    memset(buffer, 0, sizeof(IHS_Buffer));
    buffer->initialCapacity = initialCapacity;
    buffer->maxCapacity = maxCapacity;
}

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
        newCapacity = 1024;
    }
    while (newCapacity < wantedCapacity) {
        newCapacity *= 2;
    }
    IHS_BufferEnsureCapacityExact(buffer, newCapacity);
}

void IHS_BufferClear(IHS_Buffer *buffer, bool freeData) {
    buffer->capacity = 0;
    buffer->size = 0;
    buffer->offset = 0;
    if (freeData && buffer->data != NULL) {
        free(buffer->data);
        buffer->data = NULL;
    }
}

void IHS_BufferOffsetBy(IHS_Buffer *buffer, int offset) {
    assert(offset <= buffer->size);
    buffer->offset += offset;
    buffer->size -= offset;
}

uint8_t *IHS_BufferPointer(const IHS_Buffer *buffer) {
    return IHS_BufferPointerAt(buffer, 0);
}

uint8_t *IHS_BufferPointerAt(const IHS_Buffer *buffer, size_t position) {
    assert(position >= 0 && position < (buffer->capacity - buffer->offset));
    return &buffer->data[buffer->offset + position];
}

uint8_t *IHS_BufferPointerForAppend(IHS_Buffer *buffer, size_t appendSize) {
    size_t newSize = buffer->size + appendSize;
    IHS_BufferEnsureCapacity(buffer, buffer->offset + newSize);
    return IHS_BufferPointerAt(buffer, buffer->size);
}

void IHS_BufferAppendMem(IHS_Buffer *buffer, const uint8_t *data, size_t dataLen) {
    uint8_t *dst = IHS_BufferPointerForAppend(buffer, dataLen);
    memcpy(dst, data, dataLen);
    buffer->size += dataLen;
}

void IHS_BufferCopyToMem(const IHS_Buffer *buffer, uint8_t *dest, size_t len) {
    assert(len <= buffer->size);
    memcpy(dest, buffer->data + buffer->offset, len);
}

void IHS_BufferReleaseOwnership(IHS_Buffer *buffer) {
    buffer->data = NULL;
    IHS_BufferClear(buffer, false);
}

void IHS_BufferTransferOwnership(IHS_Buffer *buffer, IHS_Buffer *to) {
    *to = *buffer;
    IHS_BufferReleaseOwnership(buffer);
}