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
#include "session_pri.h"

typedef struct IHS_SessionFramePacket {
    bool used;
    IHS_SessionPacketHeader header;
    uint8_t *body;
    size_t bodyLen;
} IHS_SessionFramePacket;

typedef struct IHS_SessionPacketsWindow IHS_SessionPacketsWindow;

typedef struct IHS_SessionFrame {
    IHS_SessionPacketHeader header;
    uint8_t *body;
    size_t bodyLen;
} IHS_SessionFrame;

IHS_SessionPacketsWindow *IHS_SessionPacketsWindowCreate(uint16_t capacity);

void IHS_SessionPacketsWindowDestroy(IHS_SessionPacketsWindow *window);

bool IHS_SessionPacketsWindowAdd(IHS_SessionPacketsWindow *window, const IHS_SessionPacket *packet);

bool IHS_SessionPacketsWindowPoll(IHS_SessionPacketsWindow *window, IHS_SessionFrame *frame);

/**
 * Discard all frames with timestamp difference between tail larger than `diff`
 * @param window
 * @param diff
 */
uint16_t IHS_SessionPacketsWindowDiscard(IHS_SessionPacketsWindow *window, uint32_t diff);

void IHS_SessionPacketsWindowReleaseFrame(IHS_SessionFrame *frame);

uint16_t IHS_SessionPacketsWindowAvailable(const IHS_SessionPacketsWindow *window);

uint16_t IHS_SessionPacketsWindowSize(const IHS_SessionPacketsWindow *window);

int IHS_SessionFrameEncrypt(IHS_Session *session, const uint8_t *in, size_t inLen, uint8_t *out, size_t *outLen,
                            uint64_t sequence);

int IHS_SessionFrameDecrypt(IHS_Session *session, const uint8_t *in, size_t inLen, uint8_t *out, size_t *outLen,
                            uint64_t expectedSequence);

int IHS_SessionFrameHMACSHA256(IHS_Session *session, const uint8_t *in, size_t inLen, uint8_t *out, size_t *outLen);