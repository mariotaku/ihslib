#pragma once

#include <stddef.h>
#include <stdint.h>
#include <protobuf-c/protobuf-c.h>

size_t IHS_WriteUInt32LE(uint8_t *out, uint32_t value);

size_t IHS_WriteUInt64LE(uint8_t *out, uint64_t value);

size_t IHS_ReadUInt32LE(const uint8_t *in, uint32_t *out);

size_t IHS_AppendUInt32LEToBuffer(uint32_t value, ProtobufCBufferSimple *buf);
