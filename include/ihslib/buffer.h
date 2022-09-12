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

#pragma once

#include <stddef.h>
#include <stdint.h>

typedef struct IHS_Buffer {
    uint8_t *data;
    size_t capacity;
    size_t size;
    size_t offset;

    size_t initialCapacity;
    size_t maxCapacity;
} IHS_Buffer;

uint8_t *IHS_BufferPointer(const IHS_Buffer *buffer);

uint8_t *IHS_BufferPointerAt(const IHS_Buffer *buffer, size_t position);

void IHS_BufferReadMem(const IHS_Buffer *buffer, size_t position, uint8_t *dest, size_t len);

/**
 * Set internal data pointer to NULL, so it will not be freed or accessed anymore
 * @param buffer Buffer to release ownership
 */
void IHS_BufferReleaseOwnership(IHS_Buffer *buffer);