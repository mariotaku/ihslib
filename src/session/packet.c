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

#include <memory.h>
#include <time.h>

#include "packet.h"
#include "endianness.h"
#include "crc32c.h"
#include "ihs_buffer.h"
#include "ihs_buffer_ext.h"

size_t IHS_SessionPacketHeaderParse(IHS_SessionPacketHeader *header, const uint8_t *src) {
    size_t offset = 0;
    header->hasCrc = (src[offset] & 0x80) == 0x80;
    header->type = src[offset] & 0x7f;
    if (header->type >= IHS_SessionPacketTypeMax) {
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

void IHS_SessionPacketHeaderSerialize(const IHS_SessionPacketHeader *header, IHS_Buffer *dest) {
    uint8_t serialized[IHS_PACKET_HEADER_SIZE];
    size_t offset = 0;
    serialized[offset++] = (header->hasCrc ? 0x80 : 0) | header->type & 0x7F;
    serialized[offset++] = header->retransmitCount;
    serialized[offset++] = header->srcConnectionId;
    serialized[offset++] = header->dstConnectionId;
    serialized[offset++] = header->channelId;
    offset += IHS_WriteSInt16LE(&serialized[offset], header->fragmentId);
    offset += IHS_WriteUInt16LE(&serialized[offset], header->packetId);
    offset += IHS_WriteUInt32LE(&serialized[offset], header->sendTimestamp);
    assert(offset == IHS_PACKET_HEADER_SIZE);
    IHS_BufferWriteMem(dest, 0, serialized, offset);
}

void IHS_SessionPacketBodyInitialize(IHS_Buffer *body, bool hasCrc) {
    IHS_BufferInit(body, 2048, 2048);

    // Reserve space for serialized header
    IHS_BufferFillMem(body, 0, 0, IHS_PACKET_HEADER_SIZE);
    IHS_BufferOffsetBy(body, IHS_PACKET_HEADER_SIZE);
    assert(body->offset == IHS_PACKET_HEADER_SIZE);
    if (hasCrc) {
        IHS_BufferSetSuffixLength(body, 4);
        assert(body->suffix == 4);
    }
}

IHS_SessionPacketReturn IHS_SessionPacketParse(IHS_SessionPacket *packet, IHS_Buffer *src) {
    memset(packet, 0, sizeof(IHS_SessionPacket));
    size_t headLen = IHS_SessionPacketHeaderParse(&packet->header, IHS_BufferPointer(src));
    if (!headLen) return IHS_SessionPacketResultBadHeader;
    size_t bodyLen = src->size - headLen;
    if (packet->header.hasCrc) {
        bodyLen -= 4;
        IHS_ReadUInt32LE(IHS_BufferPointerAt(src, headLen + bodyLen), &packet->crc);
        if (IHS_CRC32C(IHS_BufferPointerAt(src, 0), headLen + bodyLen) != packet->crc) {
            return IHS_SessionPacketResultBadChecksum;
        }
    }
    IHS_BufferOffsetBy(src, (int) headLen);
    src->size = bodyLen;
    IHS_BufferTransferOwnership(src, &packet->body);
    return IHS_SessionPacketResultOK;
}

void IHS_SessionPacketPadTo(IHS_SessionPacket *packet, size_t padTo) {
    size_t curSize = IHS_PACKET_HEADER_SIZE + packet->body.size;
    if (padTo <= curSize) return;
    IHS_BufferFillMem(&packet->body, packet->body.size, 0xFE, padTo - curSize);
}

void IHS_SessionPacketPopulateBuffer(IHS_SessionPacket *packet) {
    assert(packet->body.offset == IHS_PACKET_HEADER_SIZE);
    // Move write index to 0 and write header
    IHS_BufferOffsetBy(&packet->body, -IHS_PACKET_HEADER_SIZE);
    IHS_SessionPacketHeaderSerialize(&packet->header, &packet->body);

    // Write 4 bytes CRC at the end of buffer
    if (packet->header.hasCrc) {
        assert(packet->body.suffix == 4);
        uint32_t crc = IHS_CRC32C(IHS_BufferPointer(&packet->body), packet->body.size);
        IHS_WriteUInt32LE(IHS_BufferSuffixPointer(&packet->body), crc);
    }
    IHS_BufferOffsetBy(&packet->body, IHS_PACKET_HEADER_SIZE);
    assert(packet->body.offset == IHS_PACKET_HEADER_SIZE);
    assert(IHS_SessionPacketSize(packet) == IHS_BufferUsedSize(&packet->body));
}

size_t IHS_SessionPacketSize(const IHS_SessionPacket *packet) {
    return IHS_PACKET_HEADER_SIZE + packet->body.size + (packet->header.hasCrc ? 4 : 0);
}

void IHS_SessionPacketClear(IHS_SessionPacket *packet, bool freeData) {
    IHS_BufferClear(&packet->body, freeData);
}

uint32_t IHS_SessionPacketTimestamp() {
    struct timespec tp;
    clock_gettime(CLOCK_MONOTONIC, &tp);
    uint64_t nsec = tp.tv_nsec * 65536 / 1000000000;
    uint32_t sec = tp.tv_sec * 65536;
    return sec + nsec;
}