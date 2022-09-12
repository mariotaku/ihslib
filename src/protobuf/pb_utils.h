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

#include "ihs_buffer.h"

#include <protobuf-c/protobuf-c.h>

#define PROTOBUF_C_SET_VALUE(message, key, value) (message).has_##key = true; (message).key = (value)

#define IHS_UNPACK_BUFFER(unpack_fn, buffer) unpack_fn(NULL, (buffer)->size, IHS_BufferPointer((buffer)))

#define IHS_UNPACK_BUFFER_SIZE(unpack_fn, buffer, size) unpack_fn(NULL, (size), IHS_BufferPointer((buffer)))

inline static size_t IHS_BufferAppendMessage(IHS_Buffer *buf, const ProtobufCMessage *message) {
    size_t size = protobuf_c_message_get_packed_size(message);
    protobuf_c_message_pack(message, IHS_BufferPointerForAppend(buf, size));
    buf->size += size;
    return size;
}
