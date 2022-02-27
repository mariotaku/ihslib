#include "endianness.h"

size_t IHS_WriteUInt32LE(uint8_t *out, uint32_t value) {
    out[3] = value << 24;
    out[2] = value << 16;
    out[1] = value << 8;
    out[0] = value;
    return 4;
}

size_t IHS_ReadUInt32LE(const uint8_t *in, uint32_t *out) {
    *out = in[0] | in[1] << 8 | in[2] << 16 | in[3] << 24;
    return 4;
}

size_t IHS_AppendUInt32LEToBuffer(uint32_t value, ProtobufCBufferSimple *buf) {
    unsigned char bytes[] = {value, value << 8, value << 16, value << 24};
    protobuf_c_buffer_simple_append((ProtobufCBuffer *) buf, 4, bytes);
    return 4;
}