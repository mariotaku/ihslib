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

#include <memory.h>
#include "packet.h"
#include "endianness.h"
#include "crc32c.h"

size_t IHS_SessionPacketHeaderParse(IHS_SessionPacketHeader *header, const uint8_t *src) {
    size_t offset = 0;
    header->hasCrc = (src[offset] & 0x80) == 0x80;
    header->type = src[offset] & 0x7f;
    if (header->type > IHS_SessionPacketTypeDisconnect) {
        return 0;
    }
    offset += 1;
    header->retransmitCount = src[offset++];
    header->srcConnectionId = src[offset++];
    header->dstConnectionId = src[offset++];
    header->channelId = src[offset++];
    offset += IHS_ReadSInt16LE(&src[offset], &header->fragmentId);
    offset += IHS_ReadUInt16LE(&src[offset], &header->packetId);
    offset += IHS_ReadUInt32LE(&src[offset], &header->sendTimestamp);
    return offset;
}

size_t IHS_SessionPacketHeaderSerialize(const IHS_SessionPacketHeader *header, uint8_t *dest) {
    size_t offset = 0;
    dest[offset++] = (header->hasCrc ? 0x80 : 0) | header->type & 0x7F;
    dest[offset++] = header->retransmitCount;
    dest[offset++] = header->srcConnectionId;
    dest[offset++] = header->dstConnectionId;
    dest[offset++] = header->channelId;
    offset += IHS_WriteSInt16LE(&dest[offset], header->fragmentId);
    offset += IHS_WriteUInt16LE(&dest[offset], header->packetId);
    offset += IHS_WriteUInt32LE(&dest[offset], header->sendTimestamp);
    return offset;
}

IHS_SessionPacketReturn IHS_SessionPacketParse(IHS_SessionPacket *packet, const uint8_t *src, size_t srcLen) {
    size_t offset = 0;
    offset += IHS_SessionPacketHeaderParse(&packet->header, src);
    if (!offset) return IHS_SessionPacketResultBadHeader;
    packet->bodyLen = srcLen - offset;
    if (packet->header.hasCrc) {
        packet->bodyLen -= sizeof(uint32_t);
        packet->body = &src[offset];
    }
    offset += packet->bodyLen;
    if (packet->header.hasCrc) {
        IHS_ReadUInt32LE(&src[offset], &packet->crc);
        packet->crcOK = IHS_CRC32C(src, srcLen - sizeof(uint32_t)) == packet->crc;
    }
    return IHS_SessionPacketResultOK;
}

void IHS_SessionPacketPadTo(IHS_SessionPacket *packet, size_t padTo) {
    size_t curSize = IHS_SESSION_PACKET_SIZE + packet->bodyLen;
    if (padTo <= curSize) return;
    packet->bodyPad = padTo - curSize;
}

size_t IHS_SessionPacketSerialize(const IHS_SessionPacket *packet, uint8_t *dest) {
    size_t offset = 0;
    offset += IHS_SessionPacketHeaderSerialize(&packet->header, &dest[offset]);
    memcpy(&dest[offset], packet->body, packet->bodyLen);
    offset += packet->bodyLen;
    memset(&dest[offset], 0xFE, packet->bodyPad);
    offset += packet->bodyPad;
    if (packet->header.hasCrc) {
        offset += IHS_WriteUInt32LE(&dest[offset], IHS_CRC32C(dest, offset));
    }
    return offset;
}

size_t IHS_SessionPacketSize(const IHS_SessionPacket *packet) {
    return IHS_SESSION_PACKET_SIZE + packet->bodyLen + packet->bodyPad + (packet->header.hasCrc ? 4 : 0);
}