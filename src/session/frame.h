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

#include "packet.h"
#include "ihs_buffer.h"

typedef struct IHS_Session IHS_Session;

typedef struct IHS_SessionFrame {
    IHS_SessionPacketHeader header;
    IHS_Buffer body;
#if IHSLIB_PERFTRACE
    uint32_t packetTime;
#endif
} IHS_SessionFrame, IHS_SessionWindowItem;

typedef enum IHS_SessionFrameDecryptResult {
    IHS_SessionFrameDecryptOK = 0,
    /* Not normal state but can be ignored */
    IHS_SessionFrameDecryptOldSequence = 1,
    IHS_SessionFrameDecryptSequenceMismatch = -1,
    IHS_SessionFrameDecryptHashMismatch = -2,
    IHS_SessionFrameDecryptFailed = -3,
} IHS_SessionFrameDecryptResult;

/**
 * Initialize capacity, set offset (for header) and suffix (for CRC) of a frame buffer
 * @param body Buffer pointer
 * @param hasCrc If true, the suffix will be set to 4
 * @see IHS_SessionPacketBodyInitialize
 */
void IHS_SessionFrameBodyInitialize(IHS_Buffer *body, bool hasCrc);

void IHS_SessionFrameClear(IHS_SessionFrame *frame, bool freeData);

/*
 * Encrypt/decrypt functions
 */

int IHS_SessionFrameEncrypt(IHS_Session *session, const uint8_t *in, size_t inLen, uint8_t *out, size_t *outLen,
                            uint64_t sequence);

IHS_SessionFrameDecryptResult IHS_SessionFrameDecrypt(IHS_Session *session, const IHS_Buffer *in, IHS_Buffer *out,
                                                      uint64_t expectSequence, uint64_t *actualSequence);

int IHS_SessionFrameHMACSHA256(IHS_Session *session, const uint8_t *in, size_t inLen, uint8_t *out, size_t *outLen);