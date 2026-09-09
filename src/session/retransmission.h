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

#include "ihs_thread.h"
#include "ihs_queue.h"
#include "ihs_timer.h"
#include "packet.h"

typedef struct IHS_Session IHS_Session;

/**
 * Identity of a packet whose retransmission was cancelled, kept just long enough for a twin still
 * in the send queue to be recognised on arrival.
 */
typedef struct IHS_RetransmissionCancelled {
    IHS_SessionChannelId channelId;
    uint16_t packetId;
    uint16_t fragmentId;
    /** IHS_TimerNow() at the time of the cancel, for ageing the entry out. */
    uint64_t cancelledAt;
    bool valid;
} IHS_RetransmissionCancelled;

typedef struct IHS_SessionRetransmission {
    IHS_Session *session;
    IHS_Mutex *lock;
    IHS_Queue *queue;
    /**
     * Identities cancelled while nothing was queued under them. The next retransmission of a packet
     * is created asynchronously — the timer hands it to the send queue and only the send worker
     * calls IHS_RetransmissionQueue — so an ACK arriving in that window cancels nothing and the
     * twin, born afterwards, retransmits to the attempt limit. Ring, oldest overwritten.
     */
    IHS_RetransmissionCancelled cancelled[64];
    size_t cancelledHead;
} IHS_SessionRetransmission;

void IHS_RetransmissionInit(IHS_SessionRetransmission *retransmission, IHS_Session *session);

void IHS_RetransmissionDeinit(IHS_SessionRetransmission *retransmission);

bool IHS_RetransmissionQueue(IHS_SessionRetransmission *retransmission, IHS_SessionPacket *packet);

bool IHS_RetransmissionCancel(IHS_SessionRetransmission *retransmission, IHS_SessionChannelId channelId,
                              uint16_t packetId, uint16_t fragmentId);
