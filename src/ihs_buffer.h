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

#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include "ihslib/buffer.h"
#include "endianness.h"

#define IHS_BUFFER_INIT(initCap, maxCap) {.initialCapacity = (initCap), .maxCapacity = (maxCap)}

#define IHS_BUFFER_WRAP(d, l) {.data = (uint8_t*) (d), .capacity = (l), .size = (l)}

void IHS_BufferInit(IHS_Buffer *buffer, size_t initialCapacity, size_t maxCapacity);

/*
 * read functions
 */

static inline size_t IHS_BufferMaxSize(const IHS_Buffer *buffer) {
    return buffer->capacity - buffer->offset - buffer->suffix;
}

static inline size_t IHS_BufferUsedSize(const IHS_Buffer *buffer) {
    return buffer->offset + buffer->size + buffer->suffix;
}

static inline bool IHS_BufferIsNull(const IHS_Buffer *buffer) {
    return buffer->data == NULL;
}

/*
 * Check functions
 */

void IHS_BufferEnsureCapacityExact(IHS_Buffer *buffer, size_t wantedCapacity);

void IHS_BufferEnsureCapacity(IHS_Buffer *buffer, size_t wantedCapacity);

void IHS_BufferEnsureMaxSizeExact(IHS_Buffer *buffer, size_t maxSize);

void IHS_BufferEnsureMaxSize(IHS_Buffer *buffer, size_t maxSize);

/*
 * write functions: Enough write space will be ensured
 */

/**
 * Set size, offset and suffix to 0. Free owned memory if specified.
 * @param buffer Buffer instance
 * @param freeData If true, owned memory will be freed
 */
void IHS_BufferClear(IHS_Buffer *buffer, bool freeData);

/**
 * Move beginning index of data pointer by offset. Size will be decreased if moving towards end; increased if moving
 * towards beginning.
 * @param buffer Buffer instance
 * @param offset Move offset, can't be moved beyond beginning or actual size of the pointer
 */
void IHS_BufferOffsetBy(IHS_Buffer *buffer, int offset);

void IHS_BufferSetSuffixLength(IHS_Buffer *buffer, size_t suffixLen);

/**
 * Move offset and suffix to the total length
 * @param buffer Buffer instance
 */
void IHS_BufferExtendSize(IHS_Buffer *buffer);

uint8_t *IHS_BufferPointerForAppend(IHS_Buffer *buffer, size_t appendSize);

uint8_t *IHS_BufferSuffixPointer(IHS_Buffer *buffer);

void IHS_BufferAppend(IHS_Buffer *buffer, const IHS_Buffer *data);

void IHS_BufferAppendMem(IHS_Buffer *buffer, const uint8_t *data, size_t dataLen);

void IHS_BufferWriteMem(IHS_Buffer *buffer, size_t position, const uint8_t *src, size_t srcLen);

void IHS_BufferFillMem(IHS_Buffer *buffer, size_t position, uint8_t fill, size_t fillLen);

void IHS_BufferTransferOwnership(IHS_Buffer *buffer, IHS_Buffer *to);