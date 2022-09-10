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

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include "ihslib/buffer.h"

void IHS_BufferInit(IHS_Buffer *buffer, size_t initialCapacity, size_t maxCapacity);

void IHS_BufferEnsureCapacityExact(IHS_Buffer *buffer, size_t wantedCapacity);

void IHS_BufferEnsureCapacity(IHS_Buffer *buffer, size_t wantedCapacity);

void IHS_BufferClear(IHS_Buffer *buffer, bool freeData);

void IHS_BufferOffsetBy(IHS_Buffer *buffer, int offset);

uint8_t *IHS_BufferPointer(const IHS_Buffer *buffer);

uint8_t *IHS_BufferPointerAt(const IHS_Buffer *buffer, size_t position);

uint8_t *IHS_BufferPointerForAppend(IHS_Buffer *buffer, size_t appendSize);

void IHS_BufferAppend(IHS_Buffer *buffer, const uint8_t *data, size_t dataLen);

void IHS_BufferTakeOwnership(IHS_Buffer *to, IHS_Buffer *from);