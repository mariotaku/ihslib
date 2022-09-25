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

#include "packet_sender.h"
#include "ihs_thread.h"

#include <stdlib.h>

#define MAX_RETRANSMIT 10

static inline bool TimeToSend(uint32_t nextSend, uint32_t now);

typedef struct SenderItem {
    bool used;
    bool retransmit;
    IHS_SessionPacket packet;
    uint32_t nextSend;
} SenderItem;

struct IHS_SessionPacketSender {
    SenderItem *items;
    size_t capability;
    size_t size;
    IHS_Mutex *lock;
};

IHS_SessionPacketSender *IHS_SessionPacketSenderCreate(size_t capability) {
    IHS_SessionPacketSender *sender = calloc(1, sizeof(IHS_SessionPacketSender));
    sender->items = calloc(capability, sizeof(SenderItem));
    sender->capability = capability;
    sender->size = 0;
    sender->lock = IHS_MutexCreate();
    return sender;
}

void IHS_SessionPacketSenderDestroy(IHS_SessionPacketSender *sender) {
    IHS_MutexLock(sender->lock);
    for (int i = 0; i < sender->capability; ++i) {
        SenderItem *item = &sender->items[i];
        if (!item->used) {
            continue;
        }
        IHS_BufferClear(&item->packet.body, true);
    }
    IHS_MutexUnlock(sender->lock);
    IHS_MutexDestroy(sender->lock);
    free(sender->items);
    free(sender);
}

bool IHS_SessionPacketSenderQueue(IHS_SessionPacketSender *sender, IHS_SessionPacket *packet, bool retransmit) {
    IHS_MutexLock(sender->lock);
    bool added = false;
    for (int i = 0; i < sender->capability; ++i) {
        SenderItem *item = &sender->items[i];
        if (item->used) {
            if (packet->header.channelId == item->packet.header.channelId &&
                packet->header.packetId == item->packet.header.packetId) {
                added = false;
                break;
            }
            continue;
        }
        item->used = true;
        item->retransmit = retransmit;
        item->packet = *packet;
        IHS_BufferTransferOwnership(&packet->body, &item->packet.body);
        item->nextSend = 0;
        sender->size++;
        added = true;
        break;
    }
    IHS_MutexUnlock(sender->lock);
    return added;
}

bool IHS_SessionPacketSenderHasPacket(const IHS_SessionPacketSender *sender) {
    return sender->size > 0;
}

bool IHS_SessionPacketSenderFlush(IHS_SessionPacketSender *sender, IHS_SessionPacketSendFunction *fn, void *context) {
    uint32_t now = IHS_SessionPacketTimestamp();
    IHS_MutexLock(sender->lock);
    if (!IHS_SessionPacketSenderHasPacket(sender)) {
        IHS_MutexUnlock(sender->lock);
        return false;
    }
    size_t numSend = 0;
    for (int i = 0; i < sender->capability; ++i) {
        SenderItem *item = &sender->items[i];
        if (!item->used) {
            continue;
        }
        // Skip items that's too early to send
        if (!TimeToSend(item->nextSend, now)) {
            continue;
        }
        // Send the item and remove if it out of retry attempts
        item->packet.header.sendTimestamp = now;
        fn(&item->packet, context);
        item->nextSend = now + IHS_SESSION_PACKET_TIMESTAMP_FROM_MILLIS(100);
        // Prevent special 0 timestamp
        if (item->nextSend == 0) {
            item->nextSend--;
        }
        item->packet.header.retransmitCount++;
        // If packet doesn't need retransmission, or reached its max retransmit, remove it
        if (!item->retransmit || item->packet.header.retransmitCount > MAX_RETRANSMIT) {
            IHS_BufferClear(&item->packet.body, true);
            memset(item, 0, sizeof(SenderItem));
            sender->size--;
        }
        numSend++;
    }
    IHS_MutexUnlock(sender->lock);
    return numSend > 0;
}

bool IHS_SessionPacketSenderRemove(IHS_SessionPacketSender *sender, IHS_SessionChannelId channelId, uint16_t packetId) {
    IHS_MutexLock(sender->lock);
    bool removed = false;
    for (int i = 0; i < sender->capability; ++i) {
        SenderItem *item = &sender->items[i];
        if (!item->used) {
            continue;
        }
        if (channelId == item->packet.header.channelId && packetId == item->packet.header.packetId) {
            IHS_BufferClear(&item->packet.body, true);
            memset(item, 0, sizeof(SenderItem));
            sender->size--;
            removed = true;
            break;
        }
    }
    IHS_MutexUnlock(sender->lock);
    return removed;
}

static inline bool TimeToSend(uint32_t nextSend, uint32_t now) {
    return nextSend == 0 || (int) (now - nextSend) >= 0;
}

