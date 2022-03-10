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

#include "session/channels/channel.h"
#include "session/channels/ch_data.h"
#include "protobuf/remoteplay.pb-c.h"

typedef struct IHS_SessionVideoFrameHeader {
    uint16_t sequence;
    uint8_t flags;
    uint16_t reserved1;
    uint16_t reserved2;
} IHS_SessionVideoFrameHeader;

enum {
    VideoFrameFlagNeedStartSequence = 0x01,
    VideoFrameFlagNeedEscape = 0x02,
    VideoFrameFlagReserved1Increment = 0x04,
    VideoFrameFlagFrameFinish = 0x08,
    VideoFrameFlagKeyFrame = 0x10,
    VideoFrameFlagEncrypted = 0x20,
};

typedef struct IHS_SessionChannelVideo {
    IHS_SessionChannelData base;
    IHS_StreamVideoConfig config;
    uint16_t expectedSequence;
    bool waitingKeyFrame;
    bool finishedFrame;
} IHS_SessionChannelVideo;

IHS_SessionChannel *IHS_SessionChannelDataVideoCreate(IHS_Session *session, const CStartVideoDataMsg *message);