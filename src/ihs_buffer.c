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

void IHS_BaseBufferEnsureCapacity(IHS_BaseBuffer *buffer, size_t newSize) {
    // We set a 32MB hard limit
    assert(newSize < 32 * 1024 * 1024);
    size_t newCapacity = buffer->capacity;
    if (newCapacity == 0) {
        newCapacity = 1024 * 1024;
    }
    while (newCapacity < newSize) {
        newCapacity *= 2;
    }
    buffer->data = realloc(buffer->data, newCapacity);
    buffer->capacity = newCapacity;
    assert(buffer->data != NULL);
}

void IHS_BaseBufferClear(IHS_BaseBuffer *buffer, bool freeData) {
    buffer->capacity = 0;
    buffer->size = 0;
    if (freeData && buffer->data != NULL) {
        free(buffer->data);
        buffer->data = NULL;
    }
}

uint8_t *IHS_BaseBufferDataOffsetAt(IHS_BaseBuffer *buffer, size_t position) {
    assert(position >= 0 && position < buffer->capacity);
    return &buffer->data[position];
}

uint8_t *IHS_BaseBufferPointerForAppend(IHS_BaseBuffer *buffer, size_t appendSize) {
    size_t newSize = buffer->size + appendSize;
    IHS_BaseBufferEnsureCapacity(buffer, newSize);
    return IHS_BaseBufferDataOffsetAt(buffer, buffer->size);
}

void IHS_BaseBufferAppend(IHS_BaseBuffer *buffer, const uint8_t *data, size_t dataLen) {
    uint8_t *dst = IHS_BaseBufferPointerForAppend(buffer, dataLen);
    memcpy(dst, data, dataLen);
    buffer->size += dataLen;
}