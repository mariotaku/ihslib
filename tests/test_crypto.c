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

static void test_sha256(void) {
    static const uint8_t expected[IHS_CRYPTO_SHA256_SIZE] = {
            0xba, 0x78, 0x16, 0xbf, 0x8f, 0x01, 0xcf, 0xea, 0x41, 0x41, 0x40, 0xde, 0x5d, 0xae, 0x22, 0x23,
            0xb0, 0x03, 0x61, 0xa3, 0x96, 0x17, 0x7a, 0x9c, 0xb4, 0x10, 0xff, 0x61, 0xf2, 0x00, 0x15, 0xad,
    };
    uint8_t digest[IHS_CRYPTO_SHA256_SIZE];
    assert(IHS_CryptoSHA256((const uint8_t *) "abc", 3, digest) == 0);
    assert(memcmp(digest, expected, sizeof(digest)) == 0);
}

// RFC 7748, section 6.1
static const uint8_t alicePrivate[32] = {
        0x77, 0x07, 0x6d, 0x0a, 0x73, 0x18, 0xa5, 0x7d, 0x3c, 0x16, 0xc1, 0x72, 0x51, 0xb2, 0x66, 0x45,
        0xdf, 0x4c, 0x2f, 0x87, 0xeb, 0xc0, 0x99, 0x2a, 0xb1, 0x77, 0xfb, 0xa5, 0x1d, 0xb9, 0x2c, 0x2a,
};
static const uint8_t alicePublic[32] = {
        0x85, 0x20, 0xf0, 0x09, 0x89, 0x30, 0xa7, 0x54, 0x74, 0x8b, 0x7d, 0xdc, 0xb4, 0x3e, 0xf7, 0x5a,
        0x0d, 0xbf, 0x3a, 0x0d, 0x26, 0x38, 0x1a, 0xf4, 0xeb, 0xa4, 0xa9, 0x8e, 0xaa, 0x9b, 0x4e, 0x6a,
};
static const uint8_t bobPrivate[32] = {
        0x5d, 0xab, 0x08, 0x7e, 0x62, 0x4a, 0x8a, 0x4b, 0x79, 0xe1, 0x7f, 0x8b, 0x83, 0x80, 0x0e, 0xe6,
        0x6f, 0x3b, 0xb1, 0x29, 0x26, 0x18, 0xb6, 0xfd, 0x1c, 0x2f, 0x8b, 0x27, 0xff, 0x88, 0xe0, 0xeb,
};
static const uint8_t bobPublic[32] = {
        0xde, 0x9e, 0xdb, 0x7d, 0x7b, 0x7d, 0xc1, 0xb4, 0xd3, 0x5b, 0x61, 0xc2, 0xec, 0xe4, 0x35, 0x37,
        0x3f, 0x83, 0x43, 0xc8, 0x5b, 0x78, 0x67, 0x4d, 0xad, 0xfc, 0x7e, 0x14, 0x6f, 0x88, 0x2b, 0x4f,
};
static const uint8_t sharedSecret[32] = {
        0x4a, 0x5d, 0x9d, 0x5b, 0xa4, 0xce, 0x2d, 0xe1, 0x72, 0x8e, 0x3b, 0xf4, 0x80, 0x35, 0x0f, 0x25,
        0xe0, 0x7e, 0x21, 0xc9, 0x47, 0xd1, 0x9e, 0x33, 0x76, 0xf0, 0x9b, 0x3c, 0x1e, 0x16, 0x17, 0x42,
};

static void test_x25519(void) {
    uint8_t pub[32];
    assert(IHS_CryptoX25519PublicKey(alicePrivate, pub) == 0);
    assert(memcmp(pub, alicePublic, sizeof(pub)) == 0);
    assert(IHS_CryptoX25519PublicKey(bobPrivate, pub) == 0);
    assert(memcmp(pub, bobPublic, sizeof(pub)) == 0);

    // The key exchange is SHA-256 of the X25519 shared secret, and both sides agree on it.
    uint8_t expected[IHS_CRYPTO_SHA256_SIZE];
    assert(IHS_CryptoSHA256(sharedSecret, sizeof(sharedSecret), expected) == 0);
    uint8_t aliceSecret[IHS_CRYPTO_SHA256_SIZE], bobSecret[IHS_CRYPTO_SHA256_SIZE];
    assert(IHS_CryptoKeyExchange(alicePrivate, bobPublic, aliceSecret) == 0);
    assert(IHS_CryptoKeyExchange(bobPrivate, alicePublic, bobSecret) == 0);
    assert(memcmp(aliceSecret, expected, sizeof(expected)) == 0);
    assert(memcmp(bobSecret, expected, sizeof(expected)) == 0);

    // Low order point: the shared secret would be all zeros.
    static const uint8_t zero[32] = {0};
    assert(IHS_CryptoKeyExchange(alicePrivate, zero, aliceSecret) != 0);
}

int main(void) {
    test_sha256();
    test_x25519();
    test_round_trip();
    test_decrypt_rejects_runt_inputs();
    test_decrypt_with_iv_rejects_runt_inputs();
    printf("crypto tests OK\n");
    return 0;
}
