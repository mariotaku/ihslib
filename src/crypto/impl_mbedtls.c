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

#include "crypto.h"
#include "endianness.h"

#include <string.h>
#include <stdlib.h>

#include <mbedtls/pk.h>
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/entropy.h>

#define MIN(a, b) ((a) < (b) ? (a) : (b))

static int IHS_CryptoAES_CBC_PKCS7Pad(const uint8_t *in, size_t inLen, const uint8_t iv[16],
                                      const uint8_t *key, size_t keyLen, uint8_t *out, size_t *outLen, bool enc);

static int IHS_CryptoAES_ECB(const uint8_t *in, const uint8_t *key, size_t keyLen, uint8_t *out, bool enc);


int IHS_CryptoSymmetricEncrypt(const uint8_t *in, size_t inLen, const uint8_t *key, size_t keyLen, uint8_t *out,
                               size_t *outLen) {
    mbedtls_ctr_drbg_context ctr_drbg;
    mbedtls_ctr_drbg_init(&ctr_drbg);
    mbedtls_entropy_context entropy;
    mbedtls_entropy_init(&entropy);
    mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy,
                          (const unsigned char *) "@IHSlib@", 8);
    uint8_t iv[16];
    mbedtls_ctr_drbg_random(&ctr_drbg, iv, 16);
    mbedtls_ctr_drbg_free(&ctr_drbg);
    mbedtls_entropy_free(&entropy);
    return IHS_CryptoSymmetricEncryptWithIV(in, inLen, iv, 16, key, keyLen, true, out, outLen);
}

int IHS_CryptoSymmetricEncryptWithIV(const uint8_t *in, size_t inLen, const uint8_t *iv, size_t ivLen,
                                     const uint8_t *key, size_t keyLen, bool withIV, uint8_t *out, size_t *outLen) {

    size_t offset = 0;
    int ret;
    if (withIV) {
        uint8_t *ivEnc = malloc(ivLen);
        if ((ret = IHS_CryptoAES_ECB(iv, key, keyLen, ivEnc, true)) != 0) {
            free(ivEnc);
            return ret;
        }
        memcpy(&out[offset], ivEnc, ivLen);
        free(ivEnc);
        offset += ivLen;
    }
    size_t cipherLen = *outLen - offset;
    if ((ret = IHS_CryptoAES_CBC_PKCS7Pad(in, inLen, iv, key, keyLen, &out[offset], &cipherLen, true)) != 0) {
        return ret;
    }
    offset += cipherLen;
    *outLen = offset;
    return ret;
}

int IHS_CryptoSymmetricDecrypt(const uint8_t *in, size_t inLen, const uint8_t *key, size_t keyLen, uint8_t *out,
                               size_t *outLen) {
    uint8_t iv[16];
    IHS_CryptoAES_ECB(in, key, keyLen, iv, false);
    return IHS_CryptoSymmetricDecryptWithIV(&in[16], inLen - 16, iv, 16, key, keyLen, out, outLen);
}

int IHS_CryptoSymmetricDecryptWithIV(const uint8_t *in, size_t inLen, const uint8_t *iv, size_t ivLen,
                                     const uint8_t *key, size_t keyLen, uint8_t *out, size_t *outLen) {
    return IHS_CryptoAES_CBC_PKCS7Pad(in, inLen, iv, key, keyLen, out, outLen, false);
}


int IHS_CryptoRSAEncrypt(const uint8_t *in, size_t inLen, const uint8_t *key, size_t keyLen, uint8_t *out,
                         size_t *outLen) {
    mbedtls_pk_context pk;
    mbedtls_pk_init(&pk);
    mbedtls_ctr_drbg_context ctr_drbg;
    mbedtls_ctr_drbg_init(&ctr_drbg);
    mbedtls_entropy_context entropy;
    mbedtls_entropy_init(&entropy);
    mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy,
                          (const unsigned char *) "@IHSlib@", 8);

    int ret;
    if ((ret = mbedtls_pk_parse_public_key(&pk, key, keyLen)) != 0) {
        goto exit;
    }
    mbedtls_rsa_context *rsa = mbedtls_pk_rsa(pk);
    if ((ret = mbedtls_rsa_check_pubkey(rsa)) != 0) {
        goto exit;
    }
    if (*outLen < mbedtls_rsa_get_len(rsa)) {
        ret = MBEDTLS_ERR_RSA_OUTPUT_TOO_LARGE;
        goto exit;
    }

    mbedtls_rsa_set_padding(rsa, MBEDTLS_RSA_PKCS_V21, MBEDTLS_MD_SHA1);

#if MBEDTLS_VERSION_NUMBER >= 0x03000000
    ret = mbedtls_rsa_rsaes_oaep_encrypt(rsa, mbedtls_ctr_drbg_random, &ctr_drbg, NULL, 0, inLen,
                                         in, out);
#else
    ret = mbedtls_rsa_rsaes_oaep_encrypt(rsa, mbedtls_ctr_drbg_random, &ctr_drbg, MBEDTLS_RSA_PUBLIC, NULL, 0, inLen,
                                         in, out);
#endif
    if (ret == 0) {
        *outLen = mbedtls_rsa_get_len(rsa);
    }
    exit:
    mbedtls_ctr_drbg_free(&ctr_drbg);
    mbedtls_entropy_free(&entropy);
    mbedtls_pk_free(&pk);
    return ret;
}

static int IHS_CryptoAES_CBC_PKCS7Pad(const uint8_t *in, size_t inLen, const uint8_t iv[16], const uint8_t *key,
                                      size_t keyLen, uint8_t *out, size_t *outLen, bool enc) {
    int ret = 0;
    mbedtls_aes_context aes;
    mbedtls_aes_init(&aes);
    uint8_t block[IHS_CRYPTO_AES_BLOCK_SIZE], blockIv[IHS_CRYPTO_AES_BLOCK_SIZE];
    memcpy(blockIv, iv, IHS_CRYPTO_AES_BLOCK_SIZE);
    if (enc) {
        size_t blockCount = (inLen / IHS_CRYPTO_AES_BLOCK_SIZE + 1), writeLen = blockCount * IHS_CRYPTO_AES_BLOCK_SIZE;
        if (*outLen < writeLen) {
            ret = -1;
            goto cleanup;
        }
        mbedtls_aes_setkey_enc(&aes, key, keyLen * 8);
        for (size_t i = 0; i < writeLen; i += IHS_CRYPTO_AES_BLOCK_SIZE) {
            size_t inCopyLen = MIN(inLen - i, IHS_CRYPTO_AES_BLOCK_SIZE);
            if (inCopyLen > 0) {
                memcpy(block, &in[i], inCopyLen);
            }
            if (inCopyLen < IHS_CRYPTO_AES_BLOCK_SIZE) {
                /* Perform PKCS7 padding */
                memset(&block[inCopyLen], (uint8_t) (IHS_CRYPTO_AES_BLOCK_SIZE - inCopyLen),
                       IHS_CRYPTO_AES_BLOCK_SIZE - inCopyLen);
            }
            ret = mbedtls_aes_crypt_cbc(&aes, MBEDTLS_AES_ENCRYPT, IHS_CRYPTO_AES_BLOCK_SIZE, blockIv, block, &out[i]);
            if (ret != 0) break;
        }
        if (ret == 0) {
            *outLen = writeLen;
        }
    } else {
        if (*outLen < inLen || (inLen % IHS_CRYPTO_AES_BLOCK_SIZE) != 0) {
            ret = -1;
            goto cleanup;
        }
        mbedtls_aes_setkey_dec(&aes, key, keyLen * 8);
        for (size_t i = 0; i < inLen; i += IHS_CRYPTO_AES_BLOCK_SIZE) {
            memcpy(block, &in[i], IHS_CRYPTO_AES_BLOCK_SIZE);
            ret = mbedtls_aes_crypt_cbc(&aes, MBEDTLS_AES_DECRYPT, IHS_CRYPTO_AES_BLOCK_SIZE, blockIv, block, &out[i]);
            if (ret != 0) {
                goto cleanup;
            }
        }
        /* Remove PKCS7 padding */
        uint8_t padLen = out[inLen - 1];
        if (padLen > IHS_CRYPTO_AES_BLOCK_SIZE) {
            ret = -1;
            goto cleanup;
        }
        for (size_t i = inLen - padLen; i < inLen; i++) {
            if (out[i] != padLen) {
                ret = -1;
                goto cleanup;
            }
        }
        *outLen = inLen - padLen;
    }
    cleanup:
    mbedtls_aes_free(&aes);
    return ret;
}

static int IHS_CryptoAES_ECB(const uint8_t *in, const uint8_t *key, size_t keyLen, uint8_t *out, bool enc) {
    int ret;
    mbedtls_aes_context aes;
    mbedtls_aes_init(&aes);
    if (enc) {
        mbedtls_aes_setkey_enc(&aes, key, keyLen * 8);
    } else {
        mbedtls_aes_setkey_dec(&aes, key, keyLen * 8);
    }
    ret = mbedtls_aes_crypt_ecb(&aes, enc ? MBEDTLS_AES_ENCRYPT : MBEDTLS_AES_DECRYPT, in, out);
    mbedtls_aes_free(&aes);
    return ret;
}

uint32_t IHS_CryptoRandomUInt32() {
    mbedtls_ctr_drbg_context ctr_drbg;
    mbedtls_ctr_drbg_init(&ctr_drbg);
    mbedtls_entropy_context entropy;
    mbedtls_entropy_init(&entropy);
    mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy,
                          (const unsigned char *) "@IHSlib@", 8);
    uint8_t out[4];
    mbedtls_ctr_drbg_random(&ctr_drbg, out, 4);
    mbedtls_ctr_drbg_free(&ctr_drbg);
    mbedtls_entropy_free(&entropy);

    uint32_t ret;
    IHS_ReadUInt32LE(out, &ret);
    return ret;
}