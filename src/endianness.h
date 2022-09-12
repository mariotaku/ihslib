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
/**
 * @file endianness.h
 * @brief Read/write numbers in correct endianness
 * @todo Big endian support
 */
#pragma once

#include <stddef.h>
#include <stdint.h>
#include <memory.h>
#include <protobuf-c/protobuf-c.h>

inline static size_t IHS_WriteUInt16LE(uint8_t *out, uint16_t value) {
    *(uint16_t *) out = value;
    return sizeof(uint16_t);
}

inline static size_t IHS_WriteSInt16LE(uint8_t *out, int16_t value) {
    *(int16_t *) out = value;
    return sizeof(int16_t);
}

inline static size_t IHS_WriteUInt32LE(uint8_t *out, uint32_t value) {
    *(uint32_t *) out = value;
    return sizeof(uint32_t);
}

inline static size_t IHS_WriteUInt64LE(uint8_t *out, uint64_t value) {
    *(uint64_t *) out = value;
    return sizeof(uint64_t);
}

inline static size_t IHS_ReadUInt16LE(const uint8_t *in, uint16_t *out) {
    memcpy(out, in, sizeof(uint16_t));
    return sizeof(uint16_t);
}

inline static size_t IHS_ReadSInt16LE(const uint8_t *in, int16_t *out) {
    memcpy(out, in, sizeof(int16_t));
    return sizeof(int16_t);
}

inline static size_t IHS_ReadUInt32LE(const uint8_t *in, uint32_t *out) {
    memcpy(out, in, sizeof(uint32_t));
    return sizeof(uint32_t);
}

inline static size_t IHS_ReadUInt64LE(const uint8_t *in, uint64_t *out) {
    memcpy(out, in, sizeof(uint64_t));
    return sizeof(uint64_t);
}
