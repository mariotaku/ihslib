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

#include "ihslib/common.h"
#include "ihslib/buffer.h"

typedef struct IHS_UDPSocket IHS_UDPSocket;

typedef struct IHS_UDPPacket {
    IHS_SocketAddress address;
    IHS_Buffer buffer;
} IHS_UDPPacket;

IHS_UDPSocket *IHS_UDPSocketOpen(bool broadcast);

void IHS_UDPSocketClose(IHS_UDPSocket *socket);

/**
 * Wait for a datagram, a wakeup, or the timeout — whichever comes first.
 *
 * @param s Socket instance
 * @param packet Filled in when the return value is 1
 * @param timeoutMs Milliseconds to wait; 0 to poll, negative to wait indefinitely
 * @return 1 if \p packet was filled, 0 on timeout or on an IHS_UDPSocketUnblock wakeup, negative on
 * a socket error
 */
int IHS_UDPSocketReceive(IHS_UDPSocket *s, IHS_UDPPacket *packet, int timeoutMs);

/**
 * Wake a thread blocked in IHS_UDPSocketReceive. Safe to call from any thread, and safe to call when
 * nobody is waiting — the next receive then returns 0 immediately.
 */
bool IHS_UDPSocketUnblock(IHS_UDPSocket *s);

/**
 *
 * @param s
 * @param packet
 * @return true if succeeded
 */
bool IHS_UDPSocketSend(IHS_UDPSocket *s, const IHS_UDPPacket *packet);

