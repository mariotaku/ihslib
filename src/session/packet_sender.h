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
#include <stdbool.h>

#include "packet.h"

typedef struct IHS_SessionPacketSender IHS_SessionPacketSender;

typedef bool (IHS_SessionPacketSendFunction)(IHS_SessionPacket *packet, void *context);

IHS_SessionPacketSender *IHS_SessionPacketSenderCreate(size_t capability);

void IHS_SessionPacketSenderDestroy(IHS_SessionPacketSender *sender);

/**
 * Add packet to queue
 * @param sender
 * @param packet
 * @param retransmit
 * @return
 */
bool IHS_SessionPacketSenderQueue(IHS_SessionPacketSender *sender, IHS_SessionPacket *packet, bool retransmit);

bool IHS_SessionPacketSenderHasPacket(const IHS_SessionPacketSender *sender);

bool IHS_SessionPacketSenderFlush(IHS_SessionPacketSender *sender, IHS_SessionPacketSendFunction *fn, void *context);

bool IHS_SessionPacketSenderRemove(IHS_SessionPacketSender *sender, IHS_SessionChannelId channelId, uint16_t packetId);