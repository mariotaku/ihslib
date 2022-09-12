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

#include <protobuf-c/protobuf-c.h>

#include "ihslib/buffer.h"
#include "endianness.h"

inline static size_t IHS_BufferAppendUInt8(IHS_Buffer *buf, uint8_t value) {
    *IHS_BufferPointerForAppend(buf, 1) = value;
    buf->size += 1;
    return 1;
}

inline static size_t IHS_BufferAppendSInt16LE(IHS_Buffer *buf, int16_t value) {
    IHS_WriteSInt16LE(IHS_BufferPointerForAppend(buf, 2), value);
    buf->size += 2;
    return 2;
}

inline static size_t IHS_BufferAppendUInt16LE(IHS_Buffer *buf, uint16_t value) {
    IHS_WriteUInt16LE(IHS_BufferPointerForAppend(buf, 2), value);
    buf->size += 2;
    return 2;
}

inline static size_t IHS_BufferAppendUInt32LE(IHS_Buffer *buf, uint32_t value) {
    IHS_WriteUInt32LE(IHS_BufferPointerForAppend(buf, 4), value);
    buf->size += 4;
    return 4;
}

inline static size_t IHS_BufferAppendMessage(IHS_Buffer *buf, const ProtobufCMessage *message) {
    size_t size = protobuf_c_message_get_packed_size(message);
    protobuf_c_message_pack(message, IHS_BufferPointerForAppend(buf, size));
    buf->size += size;
    return size;
}
