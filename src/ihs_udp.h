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

#include "ihslib/common.h"
#include "ihslib/buffer.h"

typedef struct IHS_UDPSocket IHS_UDPSocket;

typedef struct IHS_UDPPacket {
    IHS_SocketAddress address;
    IHS_Buffer buffer;
} IHS_UDPPacket;

IHS_UDPSocket *IHS_UDPSocketOpen(bool broadcast);

void IHS_UDPSocketClose(IHS_UDPSocket *socket);

int IHS_UDPSocketReceive(IHS_UDPSocket *s, IHS_UDPPacket *packet);

/**
 *
 * @param s
 * @param packet
 * @return true if succeeded
 */
bool IHS_UDPSocketSend(IHS_UDPSocket *s, const IHS_UDPPacket *packet);

bool IHS_UDPSocketSetBlocking(IHS_UDPSocket *s, bool blocking);

bool IHS_UDPSocketSetRecvTimeout(IHS_UDPSocket *s, uint32_t timeoutUs);