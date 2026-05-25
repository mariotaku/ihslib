/*
 *  _____  _   _  _____  _  _  _
 * |_   _|| | | |/  ___|| |(_)| |     Steam
 *   | |  | |_| |\ `--. | | _ | |__     In-Home
 *   | |  |  _  | `--. \| || || '_ \      Streaming
 *  _| |_ | | | |/\__/ /| || || |_) |       Library
 *  \___/ \_| |_/\____/ |_||_||_.__/
 *
 * Copyright (c) 2026 Mariotaku <https://github.com/mariotaku>.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 */

#include <assert.h>
#include <string.h>
#include <stdio.h>

#include "crypto.h"

static const uint8_t key[16] = {
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
        0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
};
static const uint8_t iv[16] = {
        0xa0, 0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7,
        0xa8, 0xa9, 0xaa, 0xab, 0xac, 0xad, 0xae, 0xaf,
};

static void test_round_trip(void) {
    const uint8_t plain[] = "hello world, this is a longer message to exercise CBC blocks";
    size_t plainLen = sizeof(plain) - 1;
    uint8_t cipher[256];
    size_t cipherLen = sizeof(cipher);
    assert(IHS_CryptoSymmetricEncryptWithIV(plain, plainLen, iv, 16, key, 16, false,
                                            cipher, &cipherLen) == 0);
    uint8_t out[256];
    size_t outLen = sizeof(out);
    assert(IHS_CryptoSymmetricDecryptWithIV(cipher, cipherLen, iv, 16, key, 16, out, &outLen) == 0);
    assert(outLen == plainLen);
    assert(memcmp(plain, out, plainLen) == 0);
}

static void test_decrypt_rejects_runt_inputs(void) {
    // Regression: IHS_CryptoSymmetricDecrypt used to call CryptoAES_ECB(in, ...) for the
    // embedded IV without checking inLen >= 16, then pass inLen-16 (underflowed to
    // ~SIZE_MAX) into the CBC path. Confirm short inputs are rejected.
    uint8_t out[64];
    size_t outLen = sizeof(out);
    uint8_t buf[16] = {0};
    // Empty input
    assert(IHS_CryptoSymmetricDecrypt(buf, 0, key, 16, out, &outLen) != 0);
    // Just below one block
    assert(IHS_CryptoSymmetricDecrypt(buf, 15, key, 16, out, &outLen) != 0);
    // Exactly one block (IV alone, no ciphertext) — would have given inLen-16=0 to CBC,
    // which would then read out[-1] for PKCS7 padding.
    uint8_t justIv[16] = {0};
    assert(IHS_CryptoSymmetricDecrypt(justIv, 16, key, 16, out, &outLen) != 0);
}

static void test_decrypt_with_iv_rejects_runt_inputs(void) {
    // Regression: IHS_CryptoSymmetricDecryptWithIV with inLen=0 used to fall through to
    // CryptoAES_CBC_PKCS7Pad, which then read out[inLen-1] = out[-1] for PKCS7 extraction.
    uint8_t out[64];
    size_t outLen = sizeof(out);
    uint8_t buf[16] = {0};
    assert(IHS_CryptoSymmetricDecryptWithIV(buf, 0, iv, 16, key, 16, out, &outLen) != 0);
    // Non-multiple of block size — pre-existing check, still rejected.
    assert(IHS_CryptoSymmetricDecryptWithIV(buf, 7, iv, 16, key, 16, out, &outLen) != 0);
}

int main(void) {
    test_round_trip();
    test_decrypt_rejects_runt_inputs();
    test_decrypt_with_iv_rejects_runt_inputs();
    printf("crypto tests OK\n");
    return 0;
}
