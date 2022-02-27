#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

int IHS_CryptoSymmetricEncrypt(const uint8_t *in, size_t inLen, const uint8_t *key, size_t keyLen, uint8_t *out,
                               size_t *outLen);

int IHS_CryptoSymmetricEncryptWithIV(const uint8_t *in, size_t inLen, const uint8_t *iv, size_t ivLen,
                                     const uint8_t *key, size_t keyLen, bool withIV, uint8_t *out, size_t *outLen);

int IHS_CryptoSymmetricDecrypt(const uint8_t *in, size_t inLen, const uint8_t *key, size_t keyLen, uint8_t *out,
                               size_t *outLen);

int IHS_CryptoSymmetricDecryptWithIV(const uint8_t *in, size_t inLen, const uint8_t *iv, size_t ivLen,
                                     const uint8_t *key, size_t keyLen, uint8_t *out, size_t *outLen);

int IHS_CryptoRSAEncrypt(const uint8_t *in, size_t inLen, const uint8_t *key, size_t keyLen, uint8_t *out,
                         size_t *outLen);