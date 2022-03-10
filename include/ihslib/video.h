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

typedef enum IHS_StreamVideoCodec {
    IHS_StreamVideoCodecNone = 0,
    IHS_StreamVideoCodecRaw = 1,
    IHS_StreamVideoCodecVP8 = 2,
    IHS_StreamVideoCodecVP9 = 3,
    IHS_StreamVideoCodecH264 = 4,
    IHS_StreamVideoCodecHEVC = 5,
    IHS_StreamVideoCodecORBX1 = 6,
    IHS_StreamVideoCodecORBX2 = 7,
} IHS_StreamVideoCodec;

typedef enum IHS_StreamVideoFrameFlag {
    IHS_StreamVideoFrameNone = 0x00,
    IHS_StreamVideoFrameKeyFrame = 0x01,
} IHS_StreamVideoFrameFlag;

typedef struct IHS_StreamVideoConfig {
    uint32_t width, height;
    IHS_StreamVideoCodec codec;
    uint8_t *codecData;
    size_t codecDataLen;
} IHS_StreamVideoConfig;

typedef struct IHS_StreamVideoCallbacks {
    int (*start)(void *context, const IHS_StreamVideoConfig *config);

    int (*submit)(void *context, const uint8_t *data, size_t dataLen, uint16_t sequence,
                  IHS_StreamVideoFrameFlag flags);

    void (*stop)(void *context);
} IHS_StreamVideoCallbacks;