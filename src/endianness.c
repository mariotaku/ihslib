#include "endianness.h"

#define IHS_WriteInt(bytes, len, value) for(int c = 0; c < len; c++) bytes[c] = value >> (c * 8)

size_t IHS_WriteUInt32LE(uint8_t *out, uint32_t value) {
    IHS_WriteInt(out, 4, value);
    return 4;
}

size_t IHS_WriteUInt64LE(uint8_t *out, uint64_t value) {
    IHS_WriteInt(out, 8, value);
    return 8;
}

size_t IHS_ReadUInt32LE(const uint8_t *in, uint32_t *out) {
    *out = in[0] | in[1] << 8 | in[2] << 16 | in[3] << 24;
    return 4;
}

size_t IHS_AppendUInt32LEToBuffer(uint32_t value, ProtobufCBufferSimple *buf) {
    unsigned char bytes[4];
    IHS_WriteInt(bytes, 4, value);
    protobuf_c_buffer_simple_append((ProtobufCBuffer *) buf, 4, bytes);
    return 4;
}