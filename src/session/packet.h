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

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#include "ihs_buffer.h"

typedef enum IHS_SessionPacketResult {
    IHS_SessionPacketResultOK = 0,
    IHS_SessionPacketResultBadHeader = -1,
    IHS_SessionPacketResultBadChecksum = -2,
} IHS_SessionPacketReturn;

typedef enum IHS_SessionPacketType {
    IHS_SessionPacketTypeUnconnected = 0,
    IHS_SessionPacketTypeConnect = 1,
    IHS_SessionPacketTypeConnectACK = 2,
    IHS_SessionPacketTypeUnreliable = 3,
    IHS_SessionPacketTypeUnreliableFrag = 4,
    IHS_SessionPacketTypeReliable = 5,
    IHS_SessionPacketTypeReliableFrag = 6,
    IHS_SessionPacketTypeACK = 7,
    IHS_SessionPacketTypeNACK = 8,
    IHS_SessionPacketTypeDisconnect = 9,
    IHS_SessionPacketTypeUnreliableNack = 10,
    IHS_SessionPacketTypeMax,
} IHS_SessionPacketType;

typedef enum IHS_SessionChannelId {
    IHS_SessionChannelIdDiscovery = 0,
    IHS_SessionChannelIdControl = 1,
    IHS_SessionChannelIdStats = 2,
    IHS_SessionChannelIdDataStart = 3,
    IHS_SessionChannelIdMax = 0xff,
} IHS_SessionChannelId;

typedef struct IHS_SessionPacketHeader {
    /**
     * \p true if this packet has CRC. If a packet has CRC, its body MUST contain 4 bytes suffix.
     */
    bool hasCrc;
    IHS_SessionPacketType type;
    uint8_t retransmitCount;
    uint8_t srcConnectionId;
    uint8_t dstConnectionId;
    IHS_SessionChannelId channelId;
    int16_t fragmentId;
    uint16_t packetId;
    uint32_t sendTimestamp;
} IHS_SessionPacketHeader;

typedef struct IHS_SessionPacket {
    IHS_SessionPacketHeader header;
    IHS_Buffer body;
    uint32_t crc;
} IHS_SessionPacket;

#define IHS_PACKET_HEADER_SIZE 13

/**
 * This is used for something like control messages. For ACK and CONNECT message, they will not use self-increasing ID
 */
#define IHS_PACKET_ID_NEXT (-1)

#define IHS_SESSION_PACKET_TIMESTAMP_FROM_MILLIS(millis) ((uint32_t) (((uint64_t) (millis)) * 65536 / 1000))

#define IHS_SESSION_PACKET_TIMESTAMP_TO_MILLIS(diff) ((uint32_t) (((uint64_t) (diff)) * 1000 / 65536))

/**
 * Parse packet from source buffer
 * @param header Packet header
 * @param src Source buffer
 * @return Size read (should be 13), 0 if failed
 */
size_t IHS_SessionPacketHeaderParse(IHS_SessionPacketHeader *header, const uint8_t *src);

/**
 * write packet header to start of the buffer
 * @param header Packet header
 * @param dest Destination buffer
 */
void IHS_SessionPacketHeaderSerialize(const IHS_SessionPacketHeader *header, IHS_Buffer *dest);

/**
 * Initialize capacity, set offset (for header) and suffix (for CRC) of a packet buffer
 * @param body Buffer pointer
 * @param hasCrc If true, the suffix will be set to 4
 * @see IHS_SessionFrameBodyInitialize
 */
void IHS_SessionPacketBodyInitialize(IHS_Buffer *body, bool hasCrc);


/**
 * Parse session packet from body of a UDP packet
 * @param packet Pointer to store parsed packet
 * @param src UDP body buffer. Memory ownership of this buffer will be taken
 * @return Parse result
 */
IHS_SessionPacketReturn IHS_SessionPacketParse(IHS_SessionPacket *packet, IHS_Buffer *src);

void IHS_SessionPacketPadTo(IHS_SessionPacket *packet, size_t padTo);

/**
 * Update header bytes and CRC for packet
 * @param packet Packet pointer
 */
void IHS_SessionPacketPopulateBuffer(IHS_SessionPacket *packet);

size_t IHS_SessionPacketSize(const IHS_SessionPacket *packet);

void IHS_SessionPacketClear(IHS_SessionPacket *packet, bool freeData);

uint32_t IHS_SessionPacketTimestamp();