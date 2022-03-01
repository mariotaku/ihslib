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

#include "endianness.h"

#define IHS_WriteInt(bytes, len, value) for(int c = 0; c < len; c++) bytes[c] = value >> (c * 8)

size_t IHS_WriteUInt32LE(uint8_t *out, uint32_t value) {
    IHS_WriteInt(out, 4, value);
    return 4;
}

size_t IHS_WriteUInt64LE(uint8_t *out, uint64_t value) {
    IHS_WriteInt(out, 8, value);
    return 8;
}

size_t IHS_ReadUInt32LE(const uint8_t *in, uint32_t *out) {
    *out = in[0] | in[1] << 8 | in[2] << 16 | in[3] << 24;
    return 4;
}

size_t IHS_AppendUInt32LEToBuffer(uint32_t value, ProtobufCBufferSimple *buf) {
    unsigned char bytes[4];
    IHS_WriteInt(bytes, 4, value);
    protobuf_c_buffer_simple_append((ProtobufCBuffer *) buf, 4, bytes);
    return 4;
}