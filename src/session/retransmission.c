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

#include "retransmission.h"
#include "session_pri.h"

#include <string.h>
#include <assert.h>

#define RETRANSMISSION_INTERVAL 10
#define RETRANSMISSION_ATTEMPTS 20

#define CANCELLED_RING_SIZE (sizeof(((IHS_SessionRetransmission *) 0)->cancelled) / \
                             sizeof(((IHS_SessionRetransmission *) 0)->cancelled[0]))
/**
 * How long a cancelled identity stays eligible to suppress a twin. A twin is already in the send
 * queue when its identity is recorded, so the real window is sub-millisecond; the full retransmit
 * lifetime is a generous bound that still keeps a stale entry from suppressing a legitimate
 * retransmission of a wrapped-around packetId.
 */
#define CANCELLED_MAX_AGE (RETRANSMISSION_INTERVAL * RETRANSMISSION_ATTEMPTS)

typedef struct IHS_QueueItem {
    IHS_SessionPacket packet;
    IHS_TimerTask *task;
    IHS_SessionRetransmission *retransmission;
} PendingRetransmission;

typedef struct RetransmissionQuery {
    IHS_SessionChannelId channelId;
    uint16_t packetId;
    uint16_t fragmentId;
} RetransmissionQuery;

static void RetransmissionQueueItemDestroy(PendingRetransmission *item, void *context);

static uint64_t RetransmissionTimerRun(int runCount, void *context);

static void RetransmissionTimerEnd(void *context);

static bool RetransmissionIdenticalPredicate(PendingRetransmission *item, void *context);

static bool RetransmissionPacketPredicate(PendingRetransmission *item, void *context);

static void CancelledRecord(IHS_SessionRetransmission *retransmission, IHS_SessionChannelId channelId,
                            uint16_t packetId, uint16_t fragmentId);

static bool CancelledTake(IHS_SessionRetransmission *retransmission, const IHS_SessionPacketHeader *header);

void IHS_RetransmissionInit(IHS_SessionRetransmission *retransmission, IHS_Session *session) {
    retransmission->session = session;
    retransmission->lock = IHS_MutexCreate();
    retransmission->queue = IHS_QueueCreate(sizeof(PendingRetransmission));
    memset(retransmission->cancelled, 0, sizeof(retransmission->cancelled));
    retransmission->cancelledHead = 0;
}

void IHS_RetransmissionDeinit(IHS_SessionRetransmission *retransmission) {
    IHS_MutexLock(retransmission->lock);
    // The timers must already be gone: IHS_TimerDestroy runs every task's end function, and
    // RetransmissionTimerEnd both unqueues and frees its item, so the queue is empty by now.
    // If it were not, RetransmissionQueueItemDestroy would stop the task, RetransmissionTimerEnd
    // would free the node, and IHS_QueueDestroy would free it a second time. Keep
    // IHS_TimerDestroy ahead of this call in IHS_SessionDestroy.
    assert(IHS_QueueIsEmpty(retransmission->queue));
    IHS_QueueDestroy(retransmission->queue, RetransmissionQueueItemDestroy, retransmission);
    IHS_MutexUnlock(retransmission->lock);
    IHS_MutexDestroy(retransmission->lock);
}

bool IHS_RetransmissionQueue(IHS_SessionRetransmission *retransmission, IHS_SessionPacket *packet) {
    assert(packet->body.data != NULL);
    assert(packet->body.offset == IHS_PACKET_HEADER_SIZE);
    // RetransmissionTimerRun already declines to re-queue at the limit, so this is a backstop for
    // a caller that queues an exhausted packet directly. It warns where the decision is made.
    if (packet->header.retransmitCount >= RETRANSMISSION_ATTEMPTS) {
        return false;
    }
    // A retransmission only becomes visible to Cancel here, on the send worker, long after the
    // timer decided to send it. If its ACK landed in between, this is a twin of a packet the host
    // already has. Only a retransmission can be one: a first send carries retransmitCount 0 and
    // its identity has had no chance to be cancelled.
    if (packet->header.retransmitCount > 0 && CancelledTake(retransmission, &packet->header)) {
        IHS_SessionLog(retransmission->session, IHS_LogLevelVerbose, "Retransmission",
                       "Dropping cancelled Packet(channelId=%u, packetId=%u, fragmentId=%u)",
                       packet->header.channelId, packet->header.packetId, packet->header.fragmentId);
        return false;
    }
    PendingRetransmission *pending = IHS_QueueItemObtain(retransmission->queue);
    pending->packet = *packet;
    pending->retransmission = retransmission;
    pending->packet.header.retransmitCount++;
    IHS_BufferTransferOwnership(&packet->body, &pending->packet.body);
    pending->task = IHS_TimerTaskStart(retransmission->session->base.timers, RetransmissionTimerRun, RetransmissionTimerEnd,
                                       RETRANSMISSION_INTERVAL, pending);
    IHS_MutexLock(retransmission->lock);
    IHS_QueueAppend(retransmission->queue, pending);
    IHS_MutexUnlock(retransmission->lock);
    IHS_SessionLog(retransmission->session, IHS_LogLevelVerbose, "Retransmission",
                   "Queued Packet(channelId=%u, packetId=%u, fragmentId=%u), retransmitCount=%u",
                   pending->packet.header.channelId, pending->packet.header.packetId,
                   pending->packet.header.fragmentId, pending->packet.header.retransmitCount);
    return true;
}

bool IHS_RetransmissionCancel(IHS_SessionRetransmission *retransmission, IHS_SessionChannelId channelId,
                              uint16_t packetId, uint16_t fragmentId) {
    RetransmissionQuery query = {
            .channelId = channelId,
            .packetId = packetId,
            .fragmentId = fragmentId,
    };
    IHS_MutexLock(retransmission->lock);
    PendingRetransmission *match = IHS_QueuePollBy(retransmission->queue, RetransmissionPacketPredicate, &query);
    IHS_MutexUnlock(retransmission->lock);
    if (match == NULL) {
        // Nothing queued under this identity. Either the packet was never retransmittable, or a
        // retransmission of it is mid-handoff between the timer and the send worker — indis-
        // tinguishable from here, so record it and let IHS_RetransmissionQueue decide. Recording
        // only on a miss keeps the ring from churning through every ordinary ACK.
        CancelledRecord(retransmission, channelId, packetId, fragmentId);
        return false;
    } else if (match->task != NULL) {
        IHS_SessionLog(retransmission->session, IHS_LogLevelVerbose, "Retransmission",
                       "Cancelling Packet(channelId=%u, packetId=%u, fragmentId=%u), retransmitCount=%u",
                       channelId, packetId, fragmentId, match->packet.header.retransmitCount);
        IHS_TimerTask *task = match->task;
        match->task = NULL;
        IHS_TimerTaskStopImmediate(task);
    }
    return true;
}

static void RetransmissionQueueItemDestroy(PendingRetransmission *item, void *context) {
    (void) context;
    IHS_TimerTask *task = item->task;
    item->task = NULL;
    if (task != NULL) {
        IHS_TimerTaskStopImmediate(task);
    }
}

static uint64_t RetransmissionTimerRun(int runCount, void *context) {
    (void) runCount;
    PendingRetransmission *pending = context;
    IHS_SessionRetransmission *retransmission = pending->retransmission;
    IHS_SessionPacket *packet = &pending->packet;
    bool retransmit = packet->header.retransmitCount < RETRANSMISSION_ATTEMPTS;
    if (!retransmit) {
        // Twenty unacknowledged copies of one packet is not a lossy link, it is a packet the peer
        // never accepts. This is the last trace of a control message that never landed, and the
        // decision is made here — IHS_RetransmissionQueue is not even called once we stop asking
        // for a retransmit, so warning there would say nothing.
        IHS_SessionLog(retransmission->session, IHS_LogLevelWarn, "Retransmission",
                       "Giving up on Packet(channelId=%u, packetId=%u, fragmentId=%u) after %u attempts",
                       packet->header.channelId, packet->header.packetId, packet->header.fragmentId,
                       RETRANSMISSION_ATTEMPTS);
    }
    IHS_SessionQueuePacket(retransmission->session, packet, retransmit);
    assert(packet->body.data == NULL);
    return 0;
}

static void RetransmissionTimerEnd(void *context) {
    PendingRetransmission *pending = context;
    IHS_SessionRetransmission *retransmission = pending->retransmission;
    IHS_SessionLog(retransmission->session, IHS_LogLevelVerbose, "Retransmission",
                   "Timer ended for Packet(channelId=%u, packetId=%u, fragmentId=%u), retransmitCount=%u",
                   pending->packet.header.channelId, pending->packet.header.packetId, pending->packet.header.fragmentId,
                   pending->packet.header.retransmitCount);
    IHS_MutexLock(retransmission->lock);
    IHS_QueuePollBy(retransmission->queue, RetransmissionIdenticalPredicate, pending);
    IHS_MutexUnlock(retransmission->lock);
    IHS_SessionPacketClear(&pending->packet, true);
    IHS_QueueItemFree(pending);
}

static bool RetransmissionIdenticalPredicate(PendingRetransmission *item, void *context) {
    return item == context;
}

static bool RetransmissionPacketPredicate(PendingRetransmission *item, void *context) {
    RetransmissionQuery *query = context;
    return item->packet.header.channelId == query->channelId &&
           item->packet.header.packetId == query->packetId &&
           item->packet.header.fragmentId == query->fragmentId;
}

static void CancelledRecord(IHS_SessionRetransmission *retransmission, IHS_SessionChannelId channelId,
                            uint16_t packetId, uint16_t fragmentId) {
    IHS_MutexLock(retransmission->lock);
    IHS_RetransmissionCancelled *slot = &retransmission->cancelled[retransmission->cancelledHead];
    slot->channelId = channelId;
    slot->packetId = packetId;
    slot->fragmentId = fragmentId;
    slot->cancelledAt = IHS_TimerNow();
    slot->valid = true;
    retransmission->cancelledHead = (retransmission->cancelledHead + 1) % CANCELLED_RING_SIZE;
    IHS_MutexUnlock(retransmission->lock);
}

/** Consume a matching entry, so one cancel suppresses exactly one twin. */
static bool CancelledTake(IHS_SessionRetransmission *retransmission, const IHS_SessionPacketHeader *header) {
    uint64_t now = IHS_TimerNow();
    bool taken = false;
    IHS_MutexLock(retransmission->lock);
    for (size_t i = 0; i < CANCELLED_RING_SIZE; i++) {
        IHS_RetransmissionCancelled *slot = &retransmission->cancelled[i];
        if (!slot->valid) {
            continue;
        }
        if (now - slot->cancelledAt > CANCELLED_MAX_AGE) {
            slot->valid = false;
            continue;
        }
        if (slot->channelId == header->channelId && slot->packetId == header->packetId &&
            slot->fragmentId == header->fragmentId) {
            slot->valid = false;
            taken = true;
            break;
        }
    }
    IHS_MutexUnlock(retransmission->lock);
    return taken;
}