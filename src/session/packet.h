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

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

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
} IHS_SessionPacketType;

typedef enum IHS_SessionChannelId {
    IHS_SessionChannelIdDiscovery = 0,
    IHS_SessionChannelIdControl = 1,
    IHS_SessionChannelIdStats = 2,
    IHS_SessionChannelIdDataStart = 3,
    IHS_SessionChannelIdMax = 0xff,
} IHS_SessionChannelId;

typedef struct IHS_SessionPacketHeader {
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
    const uint8_t *body;
    size_t bodyLen;
    /**
     * Padding between body and CRC
     */
    size_t bodyPad;
    uint32_t crc;
} IHS_SessionPacket;

#define IHS_PACKET_HEADER_SIZE 13

/**
 *
 * @param header Packet header
 * @param src Source buffer
 * @return Size read (should be 12), 0 if failed
 */
size_t IHS_SessionPacketHeaderParse(IHS_SessionPacketHeader *header, const uint8_t *src);

/**
 *
 * @param header Packet header
 * @param dest Destination buffer
 * @return Size written (should be 12), 0 if failed
 */
size_t IHS_SessionPacketHeaderSerialize(const IHS_SessionPacketHeader *header, uint8_t *dest);

IHS_SessionPacketReturn IHS_SessionPacketParse(IHS_SessionPacket *packet, const uint8_t *src, size_t srcLen);

void IHS_SessionPacketPadTo(IHS_SessionPacket *packet, size_t padTo);

size_t IHS_SessionPacketSerialize(const IHS_SessionPacket *packet, uint8_t *dest);

size_t IHS_SessionPacketSize(const IHS_SessionPacket *packet);