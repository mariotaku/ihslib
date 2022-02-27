#pragma once

#include <stdint.h>

//static const uint64_t deviceId = 11451419190810;
static const uint64_t deviceId = 76561198142668823;

//static const uint8_t secretKey[32] = {
//        11, 45, 14, 19, 19, 8, 1, 0,
//        11, 45, 14, 19, 19, 8, 1, 0,
//        11, 45, 14, 19, 19, 8, 1, 0,
//        11, 45, 14, 19, 19, 8, 1, 0,
//};
static const uint8_t secretKey[32] = {
        0x64, 0xb5, 0x0b, 0x75, 0xfc, 0x2e, 0x9a, 0xe8,
        0x2a, 0x15, 0xed, 0xd9, 0x35, 0x40, 0x0c, 0x49,
        0xdd, 0x37, 0xc9, 0x74, 0xb1, 0xe1, 0xb3, 0xc4,
        0xc9, 0x54, 0xb9, 0x73, 0xda, 0x26, 0xbb, 0x0b,
};

static const char *deviceName = "BABYLON STAGE34";